/**
 * @file epub_reader.h
 * @brief ePub e-book file reader.
 *
 * Provides a lightweight ePub parser for the Parchment e-reader.
 * Supports:
 *   - Opening ePub files (ZIP-based format)
 *   - Extracting book metadata (title, author)
 *   - Enumerating chapters via the OPF spine
 *   - Extracting chapter plain text (HTML tags stripped, entities decoded)
 *
 * Limitations (intentional for embedded use):
 *   - No CSS interpretation
 *   - No image rendering
 *   - Basic HTML entity decoding (&amp; &lt; &gt; &quot; &#NNN; &#xHHH;)
 *   - Assumes well-formed ePub structure
 */

#ifndef EPUB_READER_H
#define EPUB_READER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of spine items (chapters) supported. */
#define EPUB_MAX_CHAPTERS 256

/** Maximum length of metadata strings. */
#define EPUB_MAX_METADATA_LEN 256

/** Opaque book handle. */
typedef struct epub_book epub_book_t;

/* ========================================================================== */
/*  Lifecycle                                                                  */
/* ========================================================================== */

/**
 * @brief Open an ePub file.
 *
 * Parses the ZIP archive, reads container.xml and the OPF package document,
 * and builds the chapter list from the spine.
 *
 * @param filepath  Path to the .epub file.
 * @return Book handle, or NULL on failure.
 */
epub_book_t *epub_open(const char *filepath);

/**
 * @brief Close an ePub book and free all resources.
 *
 * @param book  Book handle (NULL-safe).
 */
void epub_close(epub_book_t *book);

/* ========================================================================== */
/*  Metadata                                                                   */
/* ========================================================================== */

/**
 * @brief Get the book title.
 *
 * @return Title string, or "Unknown" if not available.
 */
const char *epub_get_title(const epub_book_t *book);

/**
 * @brief Get the book author.
 *
 * @return Author string, or "Unknown" if not available.
 */
const char *epub_get_author(const epub_book_t *book);

/* ========================================================================== */
/*  Chapters                                                                   */
/* ========================================================================== */

/**
 * @brief Get the number of chapters (spine items).
 */
int epub_get_chapter_count(const epub_book_t *book);

/**
 * @brief Get the plain text content of a chapter.
 *
 * Extracts the chapter's XHTML file from the ZIP archive, strips all HTML
 * tags, decodes entities, and normalizes whitespace. Block elements
 * (<p>, <div>, <br>, <h1>-<h6>) are converted to newline characters.
 *
 * The returned string is heap-allocated; the caller must free() it.
 *
 * @param book           Book handle.
 * @param chapter_index  Chapter index (0-based).
 * @return Plain text string (caller frees), or NULL on error.
 */
char *epub_get_chapter_text(epub_book_t *book, int chapter_index);

#ifdef __cplusplus
}
#endif

#endif /* EPUB_READER_H */
