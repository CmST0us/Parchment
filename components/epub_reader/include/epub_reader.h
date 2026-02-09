/**
 * @file epub_reader.h
 * @brief ePub e-book file reader.
 *
 * Provides a lightweight ePub parser for the Parchment e-reader.
 * Supports:
 *   - Opening ePub files (ZIP-based format)
 *   - Extracting book metadata (title, author)
 *   - Enumerating chapters via the OPF spine
 *   - Extracting chapter content as structured blocks (text + images)
 *   - Image data extraction from the ZIP archive
 */

#ifndef EPUB_READER_H
#define EPUB_READER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of spine items (chapters) supported. */
#define EPUB_MAX_CHAPTERS 256

/** Maximum length of metadata strings. */
#define EPUB_MAX_METADATA_LEN 256

/** Maximum content blocks per chapter. */
#define EPUB_MAX_BLOCKS 512

/** Opaque book handle. */
typedef struct epub_book epub_book_t;

/** Content block type. */
typedef enum {
    EPUB_BLOCK_TEXT,   /**< Plain text paragraph(s). */
    EPUB_BLOCK_IMAGE,  /**< Embedded image. */
} epub_block_type_t;

/** A single content block (text or image). */
typedef struct {
    epub_block_type_t type;
    union {
        char *text;                     /**< TEXT: heap-allocated string. */
        struct {
            uint8_t *data;              /**< IMAGE: raw file data from ZIP. */
            size_t   data_len;          /**< IMAGE: data length in bytes. */
        } image;
    };
} epub_content_block_t;

/** Chapter content: array of content blocks. */
typedef struct {
    epub_content_block_t *blocks;       /**< Array of blocks. */
    int                   block_count;  /**< Number of blocks. */
} epub_chapter_content_t;

/* ========================================================================== */
/*  Lifecycle                                                                  */
/* ========================================================================== */

epub_book_t *epub_open(const char *filepath);
void epub_close(epub_book_t *book);

/* ========================================================================== */
/*  Metadata                                                                   */
/* ========================================================================== */

const char *epub_get_title(const epub_book_t *book);
const char *epub_get_author(const epub_book_t *book);

/* ========================================================================== */
/*  Chapters                                                                   */
/* ========================================================================== */

int epub_get_chapter_count(const epub_book_t *book);

/**
 * @brief Get the plain text content of a chapter (legacy API).
 *
 * The returned string is heap-allocated; the caller must free() it.
 */
char *epub_get_chapter_text(epub_book_t *book, int chapter_index);

/**
 * @brief Get structured content of a chapter (text + images).
 *
 * Returns an array of content blocks representing the chapter's
 * text paragraphs and inline images in reading order.
 *
 * @param book           Book handle.
 * @param chapter_index  Chapter index (0-based).
 * @return Chapter content, or NULL on error. Caller must call
 *         epub_free_chapter_content().
 */
epub_chapter_content_t *epub_get_chapter_content(epub_book_t *book,
                                                  int chapter_index);

/**
 * @brief Free chapter content returned by epub_get_chapter_content().
 */
void epub_free_chapter_content(epub_chapter_content_t *content);

#ifdef __cplusplus
}
#endif

#endif /* EPUB_READER_H */
