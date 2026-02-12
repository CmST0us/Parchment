/**
 * @file ui_font.c
 * @brief 字体资源管理实现。
 *
 * 所有字体统一从 LittleFS .pfnt 文件加载到 PSRAM。
 * UI 字体（20/28px）在 boot 时常驻加载，阅读字体按需加载。
 */

#include "ui_font.h"
#include "ui_font_pfnt.h"

#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include "esp_littlefs.h"
#include "esp_log.h"

static const char *TAG = "ui_font";

/** LittleFS 挂载路径。 */
#define FONTS_MOUNT_POINT "/fonts"

/** 最大可跟踪的字体文件数。 */
#define MAX_FONT_ENTRIES 8

/** UI 字体文件名前缀（用于区分 UI 字体和阅读字体）。 */
#define UI_FONT_PREFIX "ui_font_"

/** 最大常驻 UI 字体数量。 */
#define MAX_UI_FONTS 4

/** 已扫描的字体信息。 */
typedef struct {
    int  size_px;                     /**< 字号 */
    char path[280];                   /**< 文件完整路径 */
    bool is_ui;                       /**< 是否为 UI 字体 */
} font_entry_t;

/** 字体列表。 */
static font_entry_t s_font_entries[MAX_FONT_ENTRIES];
static int s_font_entry_count = 0;

/** 常驻 UI 字体（boot 时加载，永不卸载）。 */
static struct {
    int size;
    EpdFont *font;
} s_ui_fonts[MAX_UI_FONTS];
static int s_ui_font_count = 0;

/** 当前已加载的阅读字体。 */
static EpdFont *s_loaded_reading_font = NULL;
static int s_loaded_reading_size = 0;

/** LittleFS 是否已挂载。 */
static bool s_mounted = false;

/**
 * @brief 扫描 /fonts 目录下的 .pfnt 文件，填充 s_font_entries 列表。
 */
static void scan_fonts(void) {
    s_font_entry_count = 0;
    DIR *dir = opendir(FONTS_MOUNT_POINT);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot open %s for scanning", FONTS_MOUNT_POINT);
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL &&
           s_font_entry_count < MAX_FONT_ENTRIES) {
        size_t len = strlen(entry->d_name);
        if (len < 6 || strcmp(entry->d_name + len - 5, ".pfnt") != 0) {
            continue;
        }
        char path[280];
        snprintf(path, sizeof(path), "%s/%s", FONTS_MOUNT_POINT,
                 entry->d_name);
        pfnt_header_t hdr;
        if (pfnt_read_header(path, &hdr) != 0) {
            ESP_LOGW(TAG, "Skipping invalid pfnt: %s", path);
            continue;
        }
        font_entry_t *fe = &s_font_entries[s_font_entry_count];
        fe->size_px = hdr.font_size_px;
        strncpy(fe->path, path, sizeof(fe->path) - 1);
        fe->path[sizeof(fe->path) - 1] = '\0';
        fe->is_ui = (strncmp(entry->d_name, UI_FONT_PREFIX,
                             strlen(UI_FONT_PREFIX)) == 0);
        s_font_entry_count++;
        ESP_LOGI(TAG, "Found font: %s (%dpx, %s)", entry->d_name,
                 fe->size_px, fe->is_ui ? "UI" : "reading");
    }
    closedir(dir);

    /* 按字号升序排序。 */
    for (int i = 0; i < s_font_entry_count - 1; i++) {
        for (int j = i + 1; j < s_font_entry_count; j++) {
            if (s_font_entries[j].size_px < s_font_entries[i].size_px) {
                font_entry_t tmp = s_font_entries[i];
                s_font_entries[i] = s_font_entries[j];
                s_font_entries[j] = tmp;
            }
        }
    }
}

/**
 * @brief 加载所有 UI 字体到 PSRAM（常驻）。
 */
static void load_ui_fonts(void) {
    s_ui_font_count = 0;
    for (int i = 0; i < s_font_entry_count && s_ui_font_count < MAX_UI_FONTS;
         i++) {
        if (!s_font_entries[i].is_ui) {
            continue;
        }
        ESP_LOGI(TAG, "Loading UI font %dpx from %s",
                 s_font_entries[i].size_px, s_font_entries[i].path);
        EpdFont *font = pfnt_load(s_font_entries[i].path);
        if (font) {
            s_ui_fonts[s_ui_font_count].size = s_font_entries[i].size_px;
            s_ui_fonts[s_ui_font_count].font = font;
            s_ui_font_count++;
            ESP_LOGI(TAG, "UI font %dpx loaded to PSRAM",
                     s_font_entries[i].size_px);
        } else {
            ESP_LOGE(TAG, "Failed to load UI font %dpx",
                     s_font_entries[i].size_px);
        }
    }
    if (s_ui_font_count == 0) {
        ESP_LOGW(TAG, "No UI fonts loaded! Text rendering will not work.");
    }
}

void ui_font_init(void) {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = FONTS_MOUNT_POINT,
        .partition_label = "fonts",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS mount failed: %s", esp_err_to_name(ret));
        return;
    }
    s_mounted = true;

    size_t total = 0, used = 0;
    esp_littlefs_info("fonts", &total, &used);
    ESP_LOGI(TAG, "LittleFS mounted at %s (total: %u KB, used: %u KB)",
             FONTS_MOUNT_POINT, (unsigned)(total / 1024),
             (unsigned)(used / 1024));

    scan_fonts();
    load_ui_fonts();
}

/**
 * @brief 查找最接近请求字号的阅读字体条目（向下取整）。
 *
 * @return 匹配的 font_entry_t 指针，无匹配时返回 NULL。
 */
static const font_entry_t *find_nearest_reading(int size_px) {
    const font_entry_t *best = NULL;
    for (int i = 0; i < s_font_entry_count; i++) {
        if (!s_font_entries[i].is_ui &&
            s_font_entries[i].size_px <= size_px) {
            best = &s_font_entries[i];
        }
    }
    /* 如果全部大于请求字号，返回最小的阅读字体。 */
    if (!best) {
        for (int i = 0; i < s_font_entry_count; i++) {
            if (!s_font_entries[i].is_ui) {
                best = &s_font_entries[i];
                break;
            }
        }
    }
    return best;
}

/**
 * @brief 查找最接近的 UI 常驻字体。
 */
static const EpdFont *find_nearest_ui(int size_px) {
    if (s_ui_font_count == 0) {
        return NULL;
    }
    if (size_px <= s_ui_fonts[0].size) {
        return s_ui_fonts[0].font;
    }
    if (size_px >= s_ui_fonts[s_ui_font_count - 1].size) {
        return s_ui_fonts[s_ui_font_count - 1].font;
    }
    for (int i = s_ui_font_count - 1; i >= 0; i--) {
        if (s_ui_fonts[i].size <= size_px) {
            return s_ui_fonts[i].font;
        }
    }
    return s_ui_fonts[0].font;
}

const EpdFont *ui_font_get(int size_px) {
    /* 精确匹配 UI 常驻字体。 */
    for (int i = 0; i < s_ui_font_count; i++) {
        if (s_ui_fonts[i].size == size_px) {
            return s_ui_fonts[i].font;
        }
    }

    /* 尝试从阅读字体中获取。 */
    if (s_mounted) {
        const font_entry_t *fe = find_nearest_reading(size_px);
        if (fe) {
            /* 已加载且匹配，直接返回。 */
            if (s_loaded_reading_font &&
                s_loaded_reading_size == fe->size_px) {
                return s_loaded_reading_font;
            }
            /* 需要切换：卸载旧的，加载新的。 */
            if (s_loaded_reading_font) {
                ESP_LOGI(TAG, "Unloading reading font %dpx",
                         s_loaded_reading_size);
                pfnt_unload(s_loaded_reading_font);
                s_loaded_reading_font = NULL;
                s_loaded_reading_size = 0;
            }
            ESP_LOGI(TAG, "Loading reading font %dpx from %s",
                     fe->size_px, fe->path);
            s_loaded_reading_font = pfnt_load(fe->path);
            if (s_loaded_reading_font) {
                s_loaded_reading_size = fe->size_px;
                return s_loaded_reading_font;
            }
            ESP_LOGW(TAG, "Failed to load %s, falling back to UI font",
                     fe->path);
        }
    }

    /* Fallback 到最近的 UI 常驻字体。 */
    return find_nearest_ui(size_px);
}

int ui_font_list_available(int *sizes_out, int max_count) {
    if (!sizes_out || max_count <= 0) {
        return 0;
    }
    int count = 0;
    /* 列出阅读字体字号。 */
    for (int i = 0; i < s_font_entry_count && count < max_count; i++) {
        if (!s_font_entries[i].is_ui) {
            sizes_out[count++] = s_font_entries[i].size_px;
        }
    }
    return count;
}
