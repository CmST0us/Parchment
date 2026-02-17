/**
 * @file book_store.c
 * @brief 书籍管理实现 — SD 卡 TXT 文件扫描与排序。
 */

#include "book_store.h"
#include "sd_storage.h"
#include "text_encoding.h"

#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "esp_log.h"

static const char *TAG = "book_store";

/** 书籍扫描目录。 */
#define BOOKS_DIR SD_MOUNT_POINT "/book"

static book_info_t s_books[BOOK_STORE_MAX_BOOKS];
static size_t s_count = 0;

/**
 * @brief 判断文件名是否以 .txt 结尾（不区分大小写）。
 */
static bool is_txt_file(const char *name) {
    size_t len = strlen(name);
    if (len < 5) {
        return false;
    }
    const char *ext = name + len - 4;
    return (strcasecmp(ext, ".txt") == 0);
}

esp_err_t book_store_scan(void) {
    s_count = 0;

    if (!sd_storage_is_mounted()) {
        ESP_LOGW(TAG, "SD card not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    /* 若 /books 目录不存在则自动创建 */
    struct stat st_dir;
    if (stat(BOOKS_DIR, &st_dir) != 0) {
        if (mkdir(BOOKS_DIR, 0755) == 0) {
            ESP_LOGI(TAG, "Created %s", BOOKS_DIR);
        } else {
            ESP_LOGW(TAG, "Failed to create %s", BOOKS_DIR);
        }
    }

    ESP_LOGI(TAG, "Opening dir: %s", BOOKS_DIR);
    DIR *dir = opendir(BOOKS_DIR);
    if (!dir) {
        ESP_LOGW(TAG, "Failed to open %s", BOOKS_DIR);
        return ESP_ERR_INVALID_STATE;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && s_count < BOOK_STORE_MAX_BOOKS) {
        ESP_LOGI(TAG, "Entry: name=%s type=%d", entry->d_name, entry->d_type);
        /* 跳过目录 */
        if (entry->d_type == DT_DIR) {
            ESP_LOGI(TAG, "  -> skipped (directory)");
            continue;
        }
        if (!is_txt_file(entry->d_name)) {
            ESP_LOGI(TAG, "  -> skipped (not .txt)");
            continue;
        }

        book_info_t *book = &s_books[s_count];

        /* path 保持原始编码（GBK），供 fopen 使用 */
        snprintf(book->path, BOOK_PATH_MAX_LEN, "%s/%s",
                 BOOKS_DIR, entry->d_name);

        /* name 转为 UTF-8 供 UI 显示 */
        size_t name_len = strlen(entry->d_name);
        text_encoding_t enc = text_encoding_detect(entry->d_name, name_len);
        if (enc == TEXT_ENCODING_GBK) {
            size_t dst_len = BOOK_NAME_MAX_LEN - 1;
            text_encoding_gbk_to_utf8(entry->d_name, name_len,
                                       book->name, &dst_len);
            book->name[dst_len] = '\0';
        } else {
            strncpy(book->name, entry->d_name, BOOK_NAME_MAX_LEN - 1);
            book->name[BOOK_NAME_MAX_LEN - 1] = '\0';
        }

        /* 获取文件大小 */
        struct stat st;
        if (stat(book->path, &st) == 0) {
            book->size_bytes = (uint32_t)st.st_size;
        } else {
            book->size_bytes = 0;
        }

        s_count++;
    }

    closedir(dir);
    ESP_LOGI(TAG, "Scanned %zu TXT file(s)", s_count);
    return ESP_OK;
}

size_t book_store_get_count(void) {
    return s_count;
}

const book_info_t *book_store_get_book(size_t index) {
    if (index >= s_count) {
        return NULL;
    }
    return &s_books[index];
}

/** qsort 比较：按文件名升序。 */
static int cmp_by_name(const void *a, const void *b) {
    const book_info_t *ba = (const book_info_t *)a;
    const book_info_t *bb = (const book_info_t *)b;
    return strcasecmp(ba->name, bb->name);
}

/** qsort 比较：按文件大小降序。 */
static int cmp_by_size(const void *a, const void *b) {
    const book_info_t *ba = (const book_info_t *)a;
    const book_info_t *bb = (const book_info_t *)b;
    if (bb->size_bytes > ba->size_bytes) return 1;
    if (bb->size_bytes < ba->size_bytes) return -1;
    return 0;
}

void book_store_sort(book_sort_t sort) {
    if (s_count <= 1) {
        return;
    }
    switch (sort) {
        case BOOK_SORT_NAME:
            qsort(s_books, s_count, sizeof(book_info_t), cmp_by_name);
            break;
        case BOOK_SORT_SIZE:
            qsort(s_books, s_count, sizeof(book_info_t), cmp_by_size);
            break;
    }
}
