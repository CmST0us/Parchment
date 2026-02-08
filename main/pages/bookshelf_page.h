/**
 * @file bookshelf_page.h
 * @brief 书架页面。
 *
 * 扫描 SD 卡 books 目录下的 TXT 文件，显示为书名列表。
 * 点击书名进入阅读页面。
 */

#ifndef BOOKSHELF_PAGE_H
#define BOOKSHELF_PAGE_H

#include "ui_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 书架页面实例。 */
extern ui_page_t bookshelf_page;

#ifdef __cplusplus
}
#endif

#endif /* BOOKSHELF_PAGE_H */
