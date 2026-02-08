/**
 * @file font_manager.c
 * @brief 字体管理器实现。
 *
 * 使用 stb_truetype 进行 TTF/TTC 字体渲染，
 * 通过 PSRAM LRU 缓存避免重复光栅化。
 */

#include "font_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_heap_caps.h"

/* stb_truetype 实现（仅在此编译单元展开） */
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x, u)  heap_caps_malloc(x, MALLOC_CAP_SPIRAM)
#define STBTT_free(x, u)    free(x)
#include "stb_truetype.h"

static const char *TAG = "font_manager";

/* ========================================================================== */
/*  内部数据结构                                                                */
/* ========================================================================== */

/** 字体句柄内部结构。 */
struct font_handle {
    stbtt_fontinfo info;    /**< stb_truetype 字体信息。 */
    uint8_t       *data;    /**< 字体文件原始数据（PSRAM）。 */
    size_t         data_len;/**< 数据长度。 */
};

/** 字形缓存条目。 */
typedef struct {
    uint32_t  codepoint;    /**< Unicode 码点。 */
    int       size_px;      /**< 字号。 */
    uint8_t  *bitmap;       /**< 4bpp 位图数据（PSRAM）。 */
    int       width;        /**< 位图宽度。 */
    int       height;       /**< 位图高度。 */
    int       offset_x;     /**< 左偏移。 */
    int       offset_y;     /**< 上偏移。 */
    int       advance_x;    /**< 水平步进。 */
    uint32_t  lru_tick;     /**< LRU 时间戳。 */
    bool      valid;        /**< 条目是否有效。 */
} glyph_cache_entry_t;

/** 字形缓存大小。 */
#define GLYPH_CACHE_SIZE 512

/** 字形缓存。 */
static glyph_cache_entry_t s_cache[GLYPH_CACHE_SIZE];
static uint32_t s_cache_tick = 0;

/** 默认字体。 */
static struct font_handle s_default_font;
static bool s_default_loaded = false;

/* ========================================================================== */
/*  缓存操作                                                                   */
/* ========================================================================== */

/**
 * @brief 在缓存中查找字形。
 */
static glyph_cache_entry_t *cache_find(uint32_t codepoint, int size_px) {
    for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
        if (s_cache[i].valid &&
            s_cache[i].codepoint == codepoint &&
            s_cache[i].size_px == size_px) {
            s_cache[i].lru_tick = ++s_cache_tick;
            return &s_cache[i];
        }
    }
    return NULL;
}

/**
 * @brief 分配一个缓存条目（淘汰最久未用的）。
 */
static glyph_cache_entry_t *cache_alloc(void) {
    int oldest_idx = 0;
    uint32_t oldest_tick = UINT32_MAX;

    for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
        if (!s_cache[i].valid) {
            return &s_cache[i];
        }
        if (s_cache[i].lru_tick < oldest_tick) {
            oldest_tick = s_cache[i].lru_tick;
            oldest_idx = i;
        }
    }

    /* 淘汰最老条目 */
    glyph_cache_entry_t *entry = &s_cache[oldest_idx];
    if (entry->bitmap) {
        free(entry->bitmap);
        entry->bitmap = NULL;
    }
    entry->valid = false;
    return entry;
}

/**
 * @brief 清空缓存。
 */
static void cache_clear(void) {
    for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
        if (s_cache[i].bitmap) {
            free(s_cache[i].bitmap);
        }
    }
    memset(s_cache, 0, sizeof(s_cache));
    s_cache_tick = 0;
}

/* ========================================================================== */
/*  文件扫描辅助                                                                */
/* ========================================================================== */

/**
 * @brief 检查文件扩展名是否为 .ttf 或 .ttc。
 */
static bool is_font_file(const char *name) {
    size_t len = strlen(name);
    if (len < 4) return false;
    const char *ext = name + len - 4;
    return (strcasecmp(ext, ".ttf") == 0 || strcasecmp(ext, ".ttc") == 0);
}

/**
 * @brief 扫描目录下的字体文件。
 */
static void scan_directory(const char *dir_path, font_scan_result_t *result) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && result->count < FONT_MAX_SCAN_ENTRIES) {
        if (entry->d_type == DT_REG && is_font_file(entry->d_name)) {
            size_t need = strlen(dir_path) + 1 + strlen(entry->d_name) + 1;
            if (need > FONT_PATH_MAX) continue;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
            snprintf(result->paths[result->count], FONT_PATH_MAX,
                     "%s/%s", dir_path, entry->d_name);
#pragma GCC diagnostic pop
            result->count++;
        }
    }
    closedir(dir);
}

/* ========================================================================== */
/*  公开 API                                                                   */
/* ========================================================================== */

esp_err_t font_manager_init(void) {
    memset(s_cache, 0, sizeof(s_cache));
    s_default_loaded = false;

    /* 尝试加载默认字体 */
    const char *default_path = "/littlefs/wqy-microhei.ttc";
    FILE *f = fopen(default_path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "Default font not found: %s", default_path);
        return ESP_OK;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    s_default_font.data = heap_caps_malloc(fsize, MALLOC_CAP_SPIRAM);
    if (!s_default_font.data) {
        ESP_LOGE(TAG, "Failed to allocate %ld bytes for default font", fsize);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(s_default_font.data, 1, fsize, f);
    fclose(f);
    s_default_font.data_len = fsize;

    /* TTC: 获取第一个字面的偏移 */
    int offset = stbtt_GetFontOffsetForIndex(s_default_font.data, 0);
    if (offset < 0) {
        ESP_LOGE(TAG, "Invalid font file: %s", default_path);
        free(s_default_font.data);
        s_default_font.data = NULL;
        return ESP_ERR_INVALID_ARG;
    }

    if (!stbtt_InitFont(&s_default_font.info, s_default_font.data, offset)) {
        ESP_LOGE(TAG, "stbtt_InitFont failed: %s", default_path);
        free(s_default_font.data);
        s_default_font.data = NULL;
        return ESP_ERR_INVALID_ARG;
    }

    s_default_loaded = true;
    ESP_LOGI(TAG, "Default font loaded: %s (%ld bytes)", default_path, fsize);
    return ESP_OK;
}

void font_manager_deinit(void) {
    cache_clear();
    if (s_default_loaded && s_default_font.data) {
        free(s_default_font.data);
        s_default_font.data = NULL;
        s_default_loaded = false;
    }
}

font_handle_t font_manager_load(const char *path) {
    if (!path) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open font: %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    struct font_handle *font = heap_caps_malloc(sizeof(struct font_handle), MALLOC_CAP_SPIRAM);
    if (!font) {
        fclose(f);
        return NULL;
    }
    memset(font, 0, sizeof(*font));

    font->data = heap_caps_malloc(fsize, MALLOC_CAP_SPIRAM);
    if (!font->data) {
        ESP_LOGE(TAG, "Failed to allocate %ld bytes for font", fsize);
        free(font);
        fclose(f);
        return NULL;
    }

    fread(font->data, 1, fsize, f);
    fclose(f);
    font->data_len = fsize;

    int offset = stbtt_GetFontOffsetForIndex(font->data, 0);
    if (offset < 0) offset = 0;

    if (!stbtt_InitFont(&font->info, font->data, offset)) {
        ESP_LOGE(TAG, "stbtt_InitFont failed: %s", path);
        free(font->data);
        free(font);
        return NULL;
    }

    ESP_LOGI(TAG, "Font loaded: %s (%ld bytes)", path, fsize);
    return font;
}

void font_manager_unload(font_handle_t font) {
    if (!font) return;
    /* 不释放默认字体 */
    if (font == &s_default_font) return;
    if (font->data) free(font->data);
    free(font);
}

font_handle_t font_manager_get_default(void) {
    return s_default_loaded ? &s_default_font : NULL;
}

void font_manager_scan(font_scan_result_t *result) {
    if (!result) return;
    result->count = 0;
    scan_directory("/littlefs", result);
    scan_directory("/sdcard/fonts", result);
}

esp_err_t font_manager_get_glyph(font_handle_t font, uint32_t codepoint,
                                  int size_px, font_glyph_t *glyph) {
    if (!font || !glyph) return ESP_ERR_INVALID_ARG;

    /* 查缓存 */
    glyph_cache_entry_t *cached = cache_find(codepoint, size_px);
    if (cached) {
        glyph->bitmap   = cached->bitmap;
        glyph->width    = cached->width;
        glyph->height   = cached->height;
        glyph->offset_x = cached->offset_x;
        glyph->offset_y = cached->offset_y;
        glyph->advance_x = cached->advance_x;
        return ESP_OK;
    }

    /* 光栅化 */
    float scale = stbtt_ScaleForPixelHeight(&font->info, size_px);

    int glyph_index = stbtt_FindGlyphIndex(&font->info, codepoint);
    if (glyph_index == 0) {
        /* 字形不存在 — 生成 tofu 块 */
        int em_w = (int)(size_px * 0.7f);
        int em_h = size_px;
        size_t bmp_size = (em_w * em_h + 1) / 2;
        uint8_t *bmp = heap_caps_calloc(1, bmp_size, MALLOC_CAP_SPIRAM);
        if (!bmp) return ESP_ERR_NO_MEM;

        /* 画空心矩形（tofu） */
        for (int y = 0; y < em_h; y++) {
            for (int x = 0; x < em_w; x++) {
                bool border = (x == 0 || x == em_w - 1 || y == 0 || y == em_h - 1);
                if (border) {
                    int idx = y * em_w + x;
                    if (idx % 2 == 0) {
                        bmp[idx / 2] |= 0x0F;
                    } else {
                        bmp[idx / 2] |= 0xF0;
                    }
                }
            }
        }

        glyph_cache_entry_t *entry = cache_alloc();
        entry->codepoint = codepoint;
        entry->size_px   = size_px;
        entry->bitmap    = bmp;
        entry->width     = em_w;
        entry->height    = em_h;
        entry->offset_x  = 0;
        entry->offset_y  = em_h;
        entry->advance_x = em_w + 2;
        entry->lru_tick  = ++s_cache_tick;
        entry->valid     = true;

        glyph->bitmap    = bmp;
        glyph->width     = em_w;
        glyph->height    = em_h;
        glyph->offset_x  = 0;
        glyph->offset_y  = em_h;
        glyph->advance_x = em_w + 2;
        return ESP_OK;
    }

    int advance_w, lsb;
    stbtt_GetGlyphHMetrics(&font->info, glyph_index, &advance_w, &lsb);

    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&font->info, glyph_index, scale, scale,
                            &x0, &y0, &x1, &y1);

    int bw = x1 - x0;
    int bh = y1 - y0;

    if (bw <= 0 || bh <= 0) {
        /* 空字形（如空格） */
        glyph->bitmap    = NULL;
        glyph->width     = 0;
        glyph->height    = 0;
        glyph->offset_x  = 0;
        glyph->offset_y  = 0;
        glyph->advance_x = (int)(advance_w * scale + 0.5f);

        glyph_cache_entry_t *entry = cache_alloc();
        entry->codepoint = codepoint;
        entry->size_px   = size_px;
        entry->bitmap    = NULL;
        entry->width     = 0;
        entry->height    = 0;
        entry->offset_x  = 0;
        entry->offset_y  = 0;
        entry->advance_x = glyph->advance_x;
        entry->lru_tick  = ++s_cache_tick;
        entry->valid     = true;
        return ESP_OK;
    }

    /* stb_truetype 渲染 8bpp → 转换为 4bpp */
    uint8_t *alpha = heap_caps_malloc(bw * bh, MALLOC_CAP_SPIRAM);
    if (!alpha) return ESP_ERR_NO_MEM;

    stbtt_MakeGlyphBitmap(&font->info, alpha, bw, bh, bw, scale, scale, glyph_index);

    /* 转换为 4bpp 打包格式：每字节 2 像素，高 4 位 = 左像素 */
    size_t bmp_size = ((size_t)bw * bh + 1) / 2;
    uint8_t *bmp = heap_caps_calloc(1, bmp_size, MALLOC_CAP_SPIRAM);
    if (!bmp) {
        free(alpha);
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < bw * bh; i++) {
        uint8_t val = alpha[i] >> 4;  /* 8bpp → 4bpp */
        if (i % 2 == 0) {
            bmp[i / 2] |= val;        /* 低 4 位 = 左像素 */
        } else {
            bmp[i / 2] |= (val << 4); /* 高 4 位 = 右像素 */
        }
    }
    free(alpha);

    /* 存入缓存 */
    glyph_cache_entry_t *entry = cache_alloc();
    entry->codepoint  = codepoint;
    entry->size_px    = size_px;
    entry->bitmap     = bmp;
    entry->width      = bw;
    entry->height     = bh;
    entry->offset_x   = x0;
    entry->offset_y   = -y0;  /* stb 的 y0 向下为正，转换为向上为正 */
    entry->advance_x  = (int)(advance_w * scale + 0.5f);
    entry->lru_tick   = ++s_cache_tick;
    entry->valid      = true;

    glyph->bitmap     = bmp;
    glyph->width      = bw;
    glyph->height     = bh;
    glyph->offset_x   = x0;
    glyph->offset_y   = -y0;
    glyph->advance_x  = entry->advance_x;
    return ESP_OK;
}

void font_manager_get_metrics(font_handle_t font, int size_px,
                               int *ascent, int *descent, int *line_gap) {
    if (!font) return;
    float scale = stbtt_ScaleForPixelHeight(&font->info, size_px);
    int a, d, lg;
    stbtt_GetFontVMetrics(&font->info, &a, &d, &lg);
    if (ascent)   *ascent   = (int)(a * scale + 0.5f);
    if (descent)  *descent  = (int)(-d * scale + 0.5f);  /* 转正值 */
    if (line_gap) *line_gap = (int)(lg * scale + 0.5f);
}
