/**
 * @file ui_font.c
 * @brief 字体渲染实现。
 *
 * 从 SD 卡加载 M5ReadPaper V2 格式的 .bin 字体文件，
 * 在 PSRAM 中构建 glyph hash map 索引，按需从 SD 读取 bitmap 数据渲染。
 *
 * .bin 文件格式:
 *   Header (134 bytes): char_count(4) + font_height(1) + version(1)
 *                        + family_name(64) + style_name(64)
 *   Glyph Table (20 bytes × char_count): 每字符的元数据
 *   Bitmap Data: 1-bit packed, MSB first, 行对齐到字节
 */

#include "ui_font.h"
#include "ui_canvas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "ui_font";

/* ========================================================================== */
/*  .bin 文件格式常量                                                          */
/* ========================================================================== */

#define BIN_HEADER_SIZE       134
#define BIN_GLYPH_ENTRY_SIZE  20
#define BIN_VERSION_V2        2
#define FONT_HEIGHT_MIN       20
#define FONT_HEIGHT_MAX       50
#define FONT_CHAR_COUNT_MAX   50000

/* ========================================================================== */
/*  Glyph 索引 Hash Map                                                       */
/* ========================================================================== */

/** Glyph 元数据（从 .bin glyph table 解析）。 */
typedef struct {
    uint16_t unicode;      /**< Unicode 码位。 */
    uint16_t advance_w;    /**< 步进宽度。 */
    uint8_t  bitmap_w;     /**< bitmap 宽度（像素）。 */
    uint8_t  bitmap_h;     /**< bitmap 高度（像素）。 */
    int8_t   x_offset;     /**< 水平偏移。 */
    int8_t   y_offset;     /**< 垂直偏移。 */
    uint32_t data_offset;  /**< bitmap 数据在文件中的偏移。 */
    uint32_t data_size;    /**< bitmap 数据长度。 */
} glyph_info_t;

/**
 * Hash map 桶节点。
 * 使用开链法处理冲突。
 */
typedef struct hash_node {
    uint16_t          key;   /**< Unicode 码位。 */
    glyph_info_t      glyph; /**< Glyph 元数据。 */
    struct hash_node  *next;  /**< 链表下一个节点。 */
} hash_node_t;

/** Hash map 桶大小（素数，适合 ~20K 条目）。 */
#define HASH_BUCKET_COUNT  4099

/** Hash map 结构。 */
typedef struct {
    hash_node_t  *buckets[HASH_BUCKET_COUNT]; /**< 桶数组。 */
    hash_node_t  *pool;                        /**< 预分配节点池。 */
    uint32_t      count;                       /**< 已存储的条目数。 */
} glyph_map_t;

/* ========================================================================== */
/*  Font Context（模块内部状态）                                                */
/* ========================================================================== */

typedef struct {
    bool         loaded;       /**< 是否已加载字体。 */
    FILE        *fp;           /**< .bin 文件句柄。 */
    uint8_t      font_height;  /**< 字体高度（像素）。 */
    uint32_t     char_count;   /**< 字符总数。 */
    glyph_map_t *map;          /**< Glyph hash map。 */
} font_ctx_t;

static font_ctx_t s_font = {0};

/* ========================================================================== */
/*  Hash Map 操作                                                              */
/* ========================================================================== */

/** 简单 hash 函数。 */
static inline uint32_t hash_fn(uint16_t key) {
    return (uint32_t)key % HASH_BUCKET_COUNT;
}

/**
 * @brief 创建 glyph hash map（PSRAM 分配）。
 *
 * @param capacity 预期条目数，用于预分配节点池。
 * @return hash map 指针，失败返回 NULL。
 */
static glyph_map_t *glyph_map_create(uint32_t capacity) {
    glyph_map_t *map = heap_caps_calloc(1, sizeof(glyph_map_t), MALLOC_CAP_SPIRAM);
    if (!map) {
        ESP_LOGE(TAG, "Failed to allocate glyph map");
        return NULL;
    }

    map->pool = heap_caps_calloc(capacity, sizeof(hash_node_t), MALLOC_CAP_SPIRAM);
    if (!map->pool) {
        ESP_LOGE(TAG, "Failed to allocate glyph node pool (%lu nodes)", (unsigned long)capacity);
        heap_caps_free(map);
        return NULL;
    }

    map->count = 0;
    return map;
}

/** @brief 释放 glyph hash map。 */
static void glyph_map_destroy(glyph_map_t *map) {
    if (!map) return;
    heap_caps_free(map->pool);
    heap_caps_free(map);
}

/**
 * @brief 向 hash map 插入一个 glyph。
 *
 * 使用预分配节点池，不单独 malloc。
 */
static void glyph_map_insert(glyph_map_t *map, const glyph_info_t *glyph) {
    hash_node_t *node = &map->pool[map->count++];
    node->key   = glyph->unicode;
    node->glyph = *glyph;

    uint32_t idx = hash_fn(glyph->unicode);
    node->next = map->buckets[idx];
    map->buckets[idx] = node;
}

/**
 * @brief 在 hash map 中查找 glyph。
 *
 * @param map       hash map。
 * @param codepoint Unicode 码位。
 * @return glyph 元数据指针，未找到返回 NULL。
 */
static const glyph_info_t *glyph_map_find(const glyph_map_t *map, uint16_t codepoint) {
    uint32_t idx = hash_fn(codepoint);
    const hash_node_t *node = map->buckets[idx];
    while (node) {
        if (node->key == codepoint) {
            return &node->glyph;
        }
        node = node->next;
    }
    return NULL;
}

/* ========================================================================== */
/*  UTF-8 解码                                                                 */
/* ========================================================================== */

/**
 * @brief 从 UTF-8 字节流解码一个 Unicode codepoint。
 *
 * 支持 BMP 范围 (U+0000~U+FFFF)。无效序列跳过并返回 U+FFFD。
 *
 * @param[in]  p     当前字节指针。
 * @param[out] out   解码的 codepoint。
 * @return 消耗的字节数。
 */
static int utf8_decode(const char *p, uint32_t *out) {
    const uint8_t *s = (const uint8_t *)p;

    if (s[0] == 0) {
        *out = 0;
        return 0;
    }

    /* 1-byte: 0xxxxxxx */
    if (s[0] < 0x80) {
        *out = s[0];
        return 1;
    }

    /* 2-byte: 110xxxxx 10xxxxxx */
    if ((s[0] & 0xE0) == 0xC0) {
        if ((s[1] & 0xC0) != 0x80) { *out = 0xFFFD; return 1; }
        *out = ((uint32_t)(s[0] & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }

    /* 3-byte: 1110xxxx 10xxxxxx 10xxxxxx */
    if ((s[0] & 0xF0) == 0xE0) {
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) { *out = 0xFFFD; return 1; }
        *out = ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    }

    /* 4-byte: 超出 BMP，跳过 */
    if ((s[0] & 0xF8) == 0xF0) {
        *out = 0xFFFD;
        return 4;
    }

    /* 无效字节 */
    *out = 0xFFFD;
    return 1;
}

/* ========================================================================== */
/*  字体加载 / 卸载                                                            */
/* ========================================================================== */

esp_err_t ui_font_load(const char *path) {
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 若已加载，先卸载 */
    if (s_font.loaded) {
        ui_font_unload();
    }

    /* 打开文件 */
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Font file not found: %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    /* 读取 header (134 bytes) */
    uint8_t header[BIN_HEADER_SIZE];
    if (fread(header, 1, BIN_HEADER_SIZE, fp) != BIN_HEADER_SIZE) {
        ESP_LOGE(TAG, "Failed to read font header");
        fclose(fp);
        return ESP_ERR_INVALID_ARG;
    }

    /* 解析 header 字段 */
    uint32_t char_count = header[0] | (header[1] << 8) | (header[2] << 16) | (header[3] << 24);
    uint8_t  font_height = header[4];
    uint8_t  version     = header[5];

    /* 验证 */
    if (version != BIN_VERSION_V2) {
        ESP_LOGE(TAG, "Unsupported font version: %d (expected %d)", version, BIN_VERSION_V2);
        fclose(fp);
        return ESP_ERR_INVALID_ARG;
    }
    if (char_count == 0 || char_count > FONT_CHAR_COUNT_MAX) {
        ESP_LOGE(TAG, "Invalid char_count: %lu", (unsigned long)char_count);
        fclose(fp);
        return ESP_ERR_INVALID_ARG;
    }
    if (font_height < FONT_HEIGHT_MIN || font_height > FONT_HEIGHT_MAX) {
        ESP_LOGE(TAG, "Invalid font_height: %d", font_height);
        fclose(fp);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Loading font: %s (height=%d, chars=%lu)",
             path, font_height, (unsigned long)char_count);

    /* 创建 hash map */
    glyph_map_t *map = glyph_map_create(char_count);
    if (!map) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    /* 读取 glyph table 到临时缓冲区 */
    size_t table_size = (size_t)char_count * BIN_GLYPH_ENTRY_SIZE;
    uint8_t *table_buf = heap_caps_malloc(table_size, MALLOC_CAP_SPIRAM);
    if (!table_buf) {
        ESP_LOGE(TAG, "Failed to allocate glyph table buffer (%u bytes)", (unsigned)table_size);
        glyph_map_destroy(map);
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    if (fread(table_buf, 1, table_size, fp) != table_size) {
        ESP_LOGE(TAG, "Failed to read glyph table");
        heap_caps_free(table_buf);
        glyph_map_destroy(map);
        fclose(fp);
        return ESP_ERR_INVALID_ARG;
    }

    /* 解析 glyph table 并填充 hash map */
    for (uint32_t i = 0; i < char_count; i++) {
        const uint8_t *entry = table_buf + i * BIN_GLYPH_ENTRY_SIZE;
        glyph_info_t glyph;
        glyph.unicode     = entry[0] | (entry[1] << 8);
        glyph.advance_w   = entry[2] | (entry[3] << 8);
        glyph.bitmap_w    = entry[4];
        glyph.bitmap_h    = entry[5];
        glyph.x_offset    = (int8_t)entry[6];
        glyph.y_offset    = (int8_t)entry[7];
        glyph.data_offset = entry[8] | (entry[9] << 8) | (entry[10] << 16) | (entry[11] << 24);
        glyph.data_size   = entry[12] | (entry[13] << 8) | (entry[14] << 16) | (entry[15] << 24);
        /* entry[16..19] reserved */

        glyph_map_insert(map, &glyph);
    }

    heap_caps_free(table_buf);

    /* 保存状态 */
    s_font.loaded      = true;
    s_font.fp          = fp;
    s_font.font_height = font_height;
    s_font.char_count  = char_count;
    s_font.map         = map;

    ESP_LOGI(TAG, "Font loaded: %lu glyphs indexed", (unsigned long)char_count);
    return ESP_OK;
}

void ui_font_unload(void) {
    if (!s_font.loaded) {
        return;
    }

    if (s_font.fp) {
        fclose(s_font.fp);
        s_font.fp = NULL;
    }

    glyph_map_destroy(s_font.map);
    s_font.map = NULL;

    s_font.loaded      = false;
    s_font.font_height = 0;
    s_font.char_count  = 0;

    ESP_LOGI(TAG, "Font unloaded");
}

/* ========================================================================== */
/*  字符渲染                                                                   */
/* ========================================================================== */

/**
 * @brief 绘制缺失字符的替代标记（矩形框）。
 */
static int draw_missing_glyph(uint8_t *fb, int x, int y, uint8_t color) {
    int w = s_font.font_height / 2;
    int h = s_font.font_height;
    ui_canvas_draw_rect(fb, x, y, w, h, color, 1);
    return w;
}

int ui_font_draw_char(uint8_t *fb, int x, int y, uint32_t codepoint, uint8_t color) {
    if (!s_font.loaded || !fb) {
        return 0;
    }

    const glyph_info_t *g = glyph_map_find(s_font.map, (uint16_t)codepoint);
    if (!g) {
        return draw_missing_glyph(fb, x, y, color);
    }

    /* 无 bitmap 数据的字符（如空格） */
    if (g->data_size == 0 || g->bitmap_w == 0 || g->bitmap_h == 0) {
        return g->advance_w;
    }

    /* 从 SD 卡读取 bitmap 数据 */
    uint8_t *bmp_buf = malloc(g->data_size);
    if (!bmp_buf) {
        ESP_LOGE(TAG, "Failed to allocate bitmap buffer (%lu bytes)", (unsigned long)g->data_size);
        return g->advance_w;
    }

    fseek(s_font.fp, g->data_offset, SEEK_SET);
    size_t read = fread(bmp_buf, 1, g->data_size, s_font.fp);
    if (read != g->data_size) {
        ESP_LOGW(TAG, "Short read for glyph U+%04X: %u/%lu",
                 (unsigned)codepoint, (unsigned)read, (unsigned long)g->data_size);
        free(bmp_buf);
        return g->advance_w;
    }

    /* 渲染 1-bit bitmap 到 framebuffer */
    int draw_x = x + g->x_offset;
    int draw_y = y + g->y_offset;
    ui_canvas_draw_bitmap_1bit(fb, draw_x, draw_y, g->bitmap_w, g->bitmap_h, bmp_buf, color);

    free(bmp_buf);
    return g->advance_w;
}

/* ========================================================================== */
/*  字符串渲染                                                                 */
/* ========================================================================== */

/**
 * @brief 判断 codepoint 是否为 CJK 字符。
 *
 * CJK 字符逐字可断行。
 */
static inline bool is_cjk(uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF)   /* CJK 统一汉字基本区 */
        || (cp >= 0x3000 && cp <= 0x303F)   /* CJK 标点 */
        || (cp >= 0x3400 && cp <= 0x4DBF)   /* CJK 扩展 A */
        || (cp >= 0xFF00 && cp <= 0xFFEF)   /* 全角字符 */
        || (cp >= 0x2000 && cp <= 0x206F);  /* 通用标点 */
}

/** @brief 判断字符是否为 ASCII 空白字符。 */
static inline bool is_space(uint32_t cp) {
    return cp == ' ' || cp == '\t';
}

int ui_font_draw_text(uint8_t *fb, int x, int y, int max_w, int line_height,
                      const char *utf8_text, uint8_t color) {
    if (!s_font.loaded || !fb || !utf8_text || max_w <= 0) {
        return 0;
    }

    int cursor_x = 0;
    int cursor_y = 0;
    const char *p = utf8_text;

    while (*p) {
        /* 处理换行符 */
        if (*p == '\n') {
            cursor_x = 0;
            cursor_y += line_height;
            p++;
            continue;
        }
        if (*p == '\r') {
            p++;
            continue;
        }

        uint32_t cp;
        int bytes = utf8_decode(p, &cp);
        if (bytes == 0) break;

        if (is_cjk(cp)) {
            /* CJK 字符：逐字可断行 */
            const glyph_info_t *g = glyph_map_find(s_font.map, (uint16_t)cp);
            int advance = g ? g->advance_w : s_font.font_height / 2;

            if (cursor_x + advance > max_w && cursor_x > 0) {
                cursor_x = 0;
                cursor_y += line_height;
            }

            ui_font_draw_char(fb, x + cursor_x, y + cursor_y, cp, color);
            cursor_x += advance;
            p += bytes;
        } else if (is_space(cp)) {
            /* 空格：直接推进 */
            const glyph_info_t *g = glyph_map_find(s_font.map, (uint16_t)cp);
            int advance = g ? g->advance_w : s_font.font_height / 4;

            if (cursor_x + advance > max_w) {
                cursor_x = 0;
                cursor_y += line_height;
            }
            /* 空格不渲染，但推进光标 */
            cursor_x += advance;
            p += bytes;
        } else {
            /* ASCII 单词：测量整个词宽，尽量不拆分 */
            const char *word_start = p;
            int word_w = 0;

            /* 扫描连续的非空白、非 CJK 字符 */
            const char *wp = p;
            while (*wp) {
                uint32_t wcp;
                int wb = utf8_decode(wp, &wcp);
                if (wb == 0 || is_space(wcp) || is_cjk(wcp) || wcp == '\n' || wcp == '\r') break;

                const glyph_info_t *wg = glyph_map_find(s_font.map, (uint16_t)wcp);
                word_w += wg ? wg->advance_w : s_font.font_height / 2;
                wp += wb;
            }

            /* 如果整词放不下且当前行有内容，先换行 */
            if (cursor_x + word_w > max_w && cursor_x > 0) {
                cursor_x = 0;
                cursor_y += line_height;
            }

            /* 逐字符渲染（如果单词比行宽还长，强制在行尾断开） */
            const char *rp = word_start;
            while (rp < wp) {
                uint32_t rcp;
                int rb = utf8_decode(rp, &rcp);
                if (rb == 0) break;

                const glyph_info_t *rg = glyph_map_find(s_font.map, (uint16_t)rcp);
                int advance = rg ? rg->advance_w : s_font.font_height / 2;

                if (cursor_x + advance > max_w && cursor_x > 0) {
                    cursor_x = 0;
                    cursor_y += line_height;
                }

                ui_font_draw_char(fb, x + cursor_x, y + cursor_y, rcp, color);
                cursor_x += advance;
                rp += rb;
            }

            p = wp;
        }
    }

    return cursor_y + line_height;
}

/* ========================================================================== */
/*  文本测量                                                                   */
/* ========================================================================== */

int ui_font_measure_text(const char *utf8_text, int max_chars) {
    if (!s_font.loaded || !utf8_text) {
        return 0;
    }

    int width = 0;
    int count = 0;
    const char *p = utf8_text;

    while (*p) {
        if (max_chars > 0 && count >= max_chars) break;

        uint32_t cp;
        int bytes = utf8_decode(p, &cp);
        if (bytes == 0) break;

        const glyph_info_t *g = glyph_map_find(s_font.map, (uint16_t)cp);
        width += g ? g->advance_w : s_font.font_height / 2;

        p += bytes;
        count++;
    }

    return width;
}

int ui_font_get_height(void) {
    return s_font.loaded ? s_font.font_height : 0;
}
