/**
 * @file book_store.c
 * @brief 书籍管理实现 — SD 卡 TXT 文件扫描与排序。
 */

#include "book_store.h"
#include "sd_storage.h"

#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "esp_log.h"

static const char *TAG = "book_store";

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

    DIR *dir = opendir(SD_MOUNT_POINT);
    if (!dir) {
        ESP_LOGW(TAG, "Failed to open %s", SD_MOUNT_POINT);
        return ESP_ERR_INVALID_STATE;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && s_count < BOOK_STORE_MAX_BOOKS) {
        /* 跳过目录 */
        if (entry->d_type == DT_DIR) {
            continue;
        }
        if (!is_txt_file(entry->d_name)) {
            continue;
        }

        book_info_t *book = &s_books[s_count];
        strncpy(book->name, entry->d_name, BOOK_NAME_MAX_LEN - 1);
        book->name[BOOK_NAME_MAX_LEN - 1] = '\0';

        snprintf(book->path, BOOK_PATH_MAX_LEN, "%s/%s",
                 SD_MOUNT_POINT, entry->d_name);

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
