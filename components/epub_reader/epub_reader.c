/**
 * @file epub_reader.c
 * @brief ePub parser implementation.
 *
 * Pipeline:
 *   1. Open ePub ZIP with miniz
 *   2. Parse META-INF/container.xml → find OPF path
 *   3. Parse OPF → extract metadata (title, author) + manifest + spine
 *   4. On chapter request: extract XHTML from ZIP → strip HTML → return text
 */

#include "epub_reader.h"
#include "miniz/miniz.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ========================================================================== */
/*  Book structure                                                             */
/* ========================================================================== */

/** OPF manifest item. */
typedef struct {
    char id[64];
    char href[256];
} manifest_item_t;

#define EPUB_MAX_MANIFEST 512

struct epub_book {
    mz_zip_archive   zip;
    bool              zip_open;

    char              title[EPUB_MAX_METADATA_LEN];
    char              author[EPUB_MAX_METADATA_LEN];

    /** Base directory of the OPF file (for resolving relative hrefs). */
    char              opf_base[256];

    /** Manifest items (id → href mapping). */
    manifest_item_t   manifest[EPUB_MAX_MANIFEST];
    int               manifest_count;

    /** Spine: ordered list of chapter file paths (resolved from manifest). */
    char              chapters[EPUB_MAX_CHAPTERS][256];
    int               chapter_count;
};

/* ========================================================================== */
/*  Minimal XML helpers                                                        */
/* ========================================================================== */

/**
 * @brief Find a substring (case-insensitive for tag matching).
 */
static const char *find_str(const char *haystack, const char *needle) {
    return strstr(haystack, needle);
}

/**
 * @brief Extract an XML attribute value.
 *
 * Searches for attr="value" starting from pos.
 * Writes the value (without quotes) to out.
 * Returns pointer past the closing quote, or NULL if not found.
 */
static const char *xml_attr(const char *pos, const char *attr,
                             char *out, int out_size) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "%s=\"", attr);

    const char *p = find_str(pos, pattern);
    if (!p) return NULL;

    p += strlen(pattern);
    const char *end = strchr(p, '"');
    if (!end) return NULL;

    int len = (int)(end - p);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, p, len);
    out[len] = '\0';

    return end + 1;
}

/**
 * @brief Extract text content between a start tag and its closing tag.
 *
 * E.g., for tag="dc:title", finds <dc:title>...</dc:title>.
 * Note: does not handle nested tags of the same name.
 */
static bool xml_text_content(const char *xml, const char *tag,
                              char *out, int out_size) {
    char open_tag[128], close_tag[128];
    snprintf(open_tag, sizeof(open_tag), "<%s", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    const char *start = find_str(xml, open_tag);
    if (!start) return false;

    /* Skip to end of opening tag */
    start = strchr(start, '>');
    if (!start) return false;
    start++;

    const char *end = find_str(start, close_tag);
    if (!end) return false;

    int len = (int)(end - start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';

    return true;
}

/* ========================================================================== */
/*  Directory path helper                                                      */
/* ========================================================================== */

/**
 * @brief Extract directory part of a path. E.g., "OEBPS/content.opf" → "OEBPS/"
 */
static void path_dirname(const char *path, char *dir, int dir_size) {
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        int len = (int)(last_slash - path + 1); /* include trailing '/' */
        if (len >= dir_size) len = dir_size - 1;
        memcpy(dir, path, len);
        dir[len] = '\0';
    } else {
        dir[0] = '\0';
    }
}

/**
 * @brief Resolve a relative href against the OPF base directory.
 */
static void resolve_href(const char *base, const char *href,
                          char *out, int out_size) {
    if (href[0] == '/') {
        /* Absolute path (rare in ePub) */
        snprintf(out, out_size, "%s", href + 1);
    } else {
        snprintf(out, out_size, "%s%s", base, href);
    }
}

/* ========================================================================== */
/*  ePub parsing steps                                                         */
/* ========================================================================== */

/**
 * @brief Step 1: Parse META-INF/container.xml to find the OPF file path.
 */
static bool parse_container_xml(epub_book_t *book, char *opf_path, int opf_path_size) {
    int idx = mz_zip_reader_locate_file(&book->zip, "META-INF/container.xml", NULL, 0);
    if (idx < 0) return false;

    size_t size;
    char *xml = (char *)mz_zip_reader_extract_to_heap(&book->zip, idx, &size, 0);
    if (!xml) return false;

    /* Null-terminate */
    char *buf = (char *)realloc(xml, size + 1);
    if (!buf) { free(xml); return false; }
    buf[size] = '\0';

    bool ok = (xml_attr(buf, "full-path", opf_path, opf_path_size) != NULL);

    free(buf);
    return ok;
}

/**
 * @brief Step 2: Parse the OPF package document.
 *
 * Extracts title, author, manifest items, and spine reading order.
 */
static bool parse_opf(epub_book_t *book, const char *opf_path) {
    /* Compute base directory for resolving relative hrefs */
    path_dirname(opf_path, book->opf_base, sizeof(book->opf_base));

    int idx = mz_zip_reader_locate_file(&book->zip, opf_path, NULL, 0);
    if (idx < 0) return false;

    size_t size;
    char *xml = (char *)mz_zip_reader_extract_to_heap(&book->zip, idx, &size, 0);
    if (!xml) return false;

    char *buf = (char *)realloc(xml, size + 1);
    if (!buf) { free(xml); return false; }
    buf[size] = '\0';

    /* --- Metadata --- */
    if (!xml_text_content(buf, "dc:title", book->title, sizeof(book->title))) {
        /* Try without namespace prefix */
        if (!xml_text_content(buf, "title", book->title, sizeof(book->title))) {
            snprintf(book->title, sizeof(book->title), "Unknown");
        }
    }

    if (!xml_text_content(buf, "dc:creator", book->author, sizeof(book->author))) {
        if (!xml_text_content(buf, "creator", book->author, sizeof(book->author))) {
            snprintf(book->author, sizeof(book->author), "Unknown");
        }
    }

    /* --- Manifest --- */
    book->manifest_count = 0;
    const char *p = buf;
    while (book->manifest_count < EPUB_MAX_MANIFEST) {
        p = find_str(p, "<item ");
        if (!p) break;

        manifest_item_t *item = &book->manifest[book->manifest_count];
        const char *end = strchr(p, '>');
        if (!end) break;

        /* Extract id and href attributes within this <item> tag */
        char tag_buf[512];
        int tag_len = (int)(end - p);
        if (tag_len >= (int)sizeof(tag_buf)) tag_len = (int)sizeof(tag_buf) - 1;
        memcpy(tag_buf, p, tag_len);
        tag_buf[tag_len] = '\0';

        bool has_id = (xml_attr(tag_buf, "id", item->id, sizeof(item->id)) != NULL);
        bool has_href = (xml_attr(tag_buf, "href", item->href, sizeof(item->href)) != NULL);

        if (has_id && has_href) {
            book->manifest_count++;
        }

        p = end + 1;
    }

    /* --- Spine --- */
    book->chapter_count = 0;
    p = find_str(buf, "<spine");
    if (p) {
        while (book->chapter_count < EPUB_MAX_CHAPTERS) {
            p = find_str(p, "<itemref");
            if (!p) break;

            /* Check if we've gone past </spine> */
            const char *spine_end = find_str(buf, "</spine>");
            if (spine_end && p > spine_end) break;

            char idref[64] = {0};
            xml_attr(p, "idref", idref, sizeof(idref));

            if (idref[0]) {
                /* Look up idref in manifest to get the file path */
                for (int i = 0; i < book->manifest_count; i++) {
                    if (strcmp(book->manifest[i].id, idref) == 0) {
                        resolve_href(book->opf_base, book->manifest[i].href,
                                     book->chapters[book->chapter_count],
                                     sizeof(book->chapters[0]));
                        book->chapter_count++;
                        break;
                    }
                }
            }

            p++;
        }
    }

    free(buf);
    return book->chapter_count > 0;
}

/* ========================================================================== */
/*  HTML → plain text extraction                                               */
/* ========================================================================== */

/**
 * @brief Decode an HTML entity starting after '&'.
 *
 * Supports: &amp; &lt; &gt; &quot; &apos; &nbsp; &#NNN; &#xHHH;
 *
 * @param p      Pointer to character after '&'.
 * @param[out] ch   Decoded character.
 * @return Number of characters consumed (including ';'), or 0 on failure.
 */
static int decode_entity(const char *p, char *ch) {
    const char *semi = strchr(p, ';');
    if (!semi || (semi - p) > 10) {
        *ch = '&';
        return 0;
    }

    int len = (int)(semi - p);
    char entity[12];
    memcpy(entity, p, len);
    entity[len] = '\0';

    if (strcmp(entity, "amp") == 0)       { *ch = '&'; }
    else if (strcmp(entity, "lt") == 0)   { *ch = '<'; }
    else if (strcmp(entity, "gt") == 0)   { *ch = '>'; }
    else if (strcmp(entity, "quot") == 0) { *ch = '"'; }
    else if (strcmp(entity, "apos") == 0) { *ch = '\''; }
    else if (strcmp(entity, "nbsp") == 0) { *ch = ' '; }
    else if (entity[0] == '#') {
        long code;
        if (entity[1] == 'x' || entity[1] == 'X') {
            code = strtol(entity + 2, NULL, 16);
        } else {
            code = strtol(entity + 1, NULL, 10);
        }
        if (code > 0 && code < 128) {
            *ch = (char)code;
        } else {
            *ch = ' '; /* Non-ASCII numeric entity → space placeholder */
        }
    } else {
        *ch = '?';
    }

    return len + 1; /* consumed chars including ';' */
}

/** Block-level HTML tags that insert line breaks. */
static bool is_block_tag(const char *tag, int len) {
    /* Normalize: lowercase comparison */
    char buf[16];
    if (len >= (int)sizeof(buf)) return false;
    for (int i = 0; i < len; i++) buf[i] = (char)tolower((unsigned char)tag[i]);
    buf[len] = '\0';

    return strcmp(buf, "p") == 0 || strcmp(buf, "div") == 0 ||
           strcmp(buf, "br") == 0 ||
           strcmp(buf, "h1") == 0 || strcmp(buf, "h2") == 0 ||
           strcmp(buf, "h3") == 0 || strcmp(buf, "h4") == 0 ||
           strcmp(buf, "h5") == 0 || strcmp(buf, "h6") == 0 ||
           strcmp(buf, "li") == 0 || strcmp(buf, "tr") == 0 ||
           strcmp(buf, "blockquote") == 0;
}

/**
 * @brief Strip HTML tags from content and return plain text.
 *
 * Block-level elements produce newline separators.
 * Inline content is concatenated. Whitespace is normalized.
 */
static char *html_to_text(const char *html, size_t html_len) {
    /* Allocate output buffer (at most same size as input) */
    char *out = (char *)malloc(html_len + 1);
    if (!out) return NULL;

    int wp = 0;        /* write position */
    bool in_tag = false;
    bool in_body = false;
    bool body_found = false;
    char tag_name[64];
    int tag_name_len = 0;
    bool tag_is_close = false;
    bool last_was_newline = true;
    bool last_was_space = true;

    for (size_t i = 0; i < html_len; i++) {
        char c = html[i];

        if (in_tag) {
            if (c == '>') {
                tag_name[tag_name_len] = '\0';

                /* Track <body> */
                if (tag_name_len >= 4) {
                    char lower[8];
                    for (int j = 0; j < 4; j++)
                        lower[j] = (char)tolower((unsigned char)tag_name[j]);
                    lower[4] = '\0';
                    if (strcmp(lower, "body") == 0) {
                        if (tag_is_close) {
                            in_body = false;
                        } else {
                            in_body = true;
                            body_found = true;
                        }
                    }
                }

                /* Insert newline for block elements */
                if (in_body || !body_found) {
                    if (is_block_tag(tag_name, tag_name_len)) {
                        if (wp > 0 && !last_was_newline) {
                            out[wp++] = '\n';
                            last_was_newline = true;
                            last_was_space = true;
                        }
                    }
                }

                in_tag = false;
            } else if (tag_name_len == 0 && c == '/') {
                tag_is_close = true;
            } else if (tag_name_len < (int)sizeof(tag_name) - 1 &&
                       c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '/') {
                tag_name[tag_name_len++] = c;
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '/') {
                /* Stop accumulating tag name at whitespace/self-close */
                /* (still inside the tag, just not collecting name anymore) */
            }
        } else {
            if (c == '<') {
                in_tag = true;
                tag_name_len = 0;
                tag_is_close = false;
            } else if (!in_body && body_found) {
                /* Past </body>, ignore */
                continue;
            } else if (c == '&') {
                char decoded;
                int consumed = decode_entity(html + i + 1, &decoded);
                if (consumed > 0) {
                    i += consumed; /* skip entity */
                }
                if (decoded == ' ' || decoded == '\t') {
                    if (!last_was_space) {
                        out[wp++] = ' ';
                        last_was_space = true;
                    }
                } else {
                    out[wp++] = decoded;
                    last_was_space = false;
                    last_was_newline = false;
                }
            } else if (c == '\n' || c == '\r') {
                /* HTML source newlines → space */
                if (!last_was_space) {
                    out[wp++] = ' ';
                    last_was_space = true;
                }
            } else if (c == ' ' || c == '\t') {
                if (!last_was_space) {
                    out[wp++] = ' ';
                    last_was_space = true;
                }
            } else {
                out[wp++] = c;
                last_was_space = false;
                last_was_newline = false;
            }
        }
    }

    out[wp] = '\0';

    /* Trim trailing whitespace */
    while (wp > 0 && (out[wp - 1] == ' ' || out[wp - 1] == '\n')) {
        out[--wp] = '\0';
    }

    return out;
}

/* ========================================================================== */
/*  Public API                                                                 */
/* ========================================================================== */

epub_book_t *epub_open(const char *filepath) {
    if (!filepath) return NULL;

    epub_book_t *book = (epub_book_t *)calloc(1, sizeof(epub_book_t));
    if (!book) return NULL;

    /* Open ZIP archive */
    memset(&book->zip, 0, sizeof(book->zip));
    if (!mz_zip_reader_init_file(&book->zip, filepath, 0)) {
        fprintf(stderr, "epub_open: failed to open ZIP: %s\n", filepath);
        free(book);
        return NULL;
    }
    book->zip_open = true;

    /* Parse container.xml → OPF path */
    char opf_path[256];
    if (!parse_container_xml(book, opf_path, sizeof(opf_path))) {
        fprintf(stderr, "epub_open: failed to parse container.xml\n");
        epub_close(book);
        return NULL;
    }

    /* Parse OPF → metadata + manifest + spine */
    if (!parse_opf(book, opf_path)) {
        fprintf(stderr, "epub_open: failed to parse OPF: %s\n", opf_path);
        epub_close(book);
        return NULL;
    }

    return book;
}

void epub_close(epub_book_t *book) {
    if (!book) return;

    if (book->zip_open) {
        mz_zip_reader_end(&book->zip);
        book->zip_open = false;
    }

    free(book);
}

const char *epub_get_title(const epub_book_t *book) {
    return book ? book->title : "Unknown";
}

const char *epub_get_author(const epub_book_t *book) {
    return book ? book->author : "Unknown";
}

int epub_get_chapter_count(const epub_book_t *book) {
    return book ? book->chapter_count : 0;
}

char *epub_get_chapter_text(epub_book_t *book, int chapter_index) {
    if (!book || chapter_index < 0 || chapter_index >= book->chapter_count) {
        return NULL;
    }

    const char *path = book->chapters[chapter_index];

    int idx = mz_zip_reader_locate_file(&book->zip, path, NULL, 0);
    if (idx < 0) {
        fprintf(stderr, "epub: chapter file not found in ZIP: %s\n", path);
        return NULL;
    }

    size_t size;
    char *html = (char *)mz_zip_reader_extract_to_heap(&book->zip, idx, &size, 0);
    if (!html) {
        fprintf(stderr, "epub: failed to extract chapter: %s\n", path);
        return NULL;
    }

    char *text = html_to_text(html, size);
    free(html);

    return text;
}
