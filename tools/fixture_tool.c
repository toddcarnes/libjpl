/* Copyright (c) 2026 Todd Carnes
 * SPDX-License-Identifier: MIT
 * See LICENSE in the project root for the full license text.
 */
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

enum { FIXTURE_ROWS = 121 };

typedef struct {
    unsigned char *bytes;
    size_t length;
} line_view;

static int load_file(const char *path, unsigned char **bytes, size_t *length) {
    FILE *file = fopen(path, "rb");
    size_t size = 0, capacity = 4096;
    unsigned char *data;
    if (!file) { fprintf(stderr, "cannot open input %s\n", path); return 1; }
    data = malloc(capacity);
    if (!data) { fclose(file); return 1; }
    for (;;) {
        size_t read_count;
        if (size == capacity) {
            unsigned char *grown;
            if (capacity > SIZE_MAX / 2) { free(data); fclose(file); return 1; }
            capacity *= 2;
            grown = realloc(data, capacity);
            if (!grown) { free(data); fclose(file); return 1; }
            data = grown;
        }
        read_count = fread(data + size, 1, capacity - size, file);
        size += read_count;
        if (read_count == 0) {
            if (ferror(file)) { fprintf(stderr, "read failed: %s\n", path); free(data); fclose(file); return 1; }
            break;
        }
    }
    if (fclose(file) != 0) { fprintf(stderr, "close failed: %s\n", path); free(data); return 1; }
    *bytes = data;
    *length = size;
    return 0;
}

static int save_file(const char *path, const unsigned char *data, size_t length) {
    FILE *file = fopen(path, "wb");
    int failed;
    if (!file) { fprintf(stderr, "cannot open output %s\n", path); return 1; }
    failed = fwrite(data, 1, length, file) != length;
    if (fclose(file) != 0) failed = 1;
    if (failed) fprintf(stderr, "write failed: %s\n", path);
    return failed;
}

static int normalize_lf(const unsigned char *src, size_t len,
                        unsigned char **result, size_t *result_len) {
    unsigned char *out = malloc(len ? len : 1);
    size_t i, n = 0;
    if (!out) return 1;
    for (i = 0; i < len; ++i) {
        if (src[i] == '\r') {
            if (i + 1 >= len || src[i + 1] != '\n') {
                fprintf(stderr, "bare CR at byte %zu (next byte %u)\n",
                        i, (unsigned)(i + 1 < len ? src[i + 1] : 0));
                free(out);
                return 1;
            }
            ++i;
            out[n++] = '\n';
        } else {
            out[n++] = src[i];
        }
    }
    *result = out;
    *result_len = n;
    return 0;
}

static int canon_copy(const char *src, const char *dst) {
    unsigned char *input, *normal;
    size_t size, normal_size;
    int rc;
    if (load_file(src, &input, &size)) return 1;
    rc = normalize_lf(input, size, &normal, &normal_size);
    free(input);
    if (rc) { fprintf(stderr, "canonicalization failed: %s\n", src); return 1; }
    rc = save_file(dst, normal, normal_size);
    free(normal);
    return rc;
}

static int flip(const char *path) {
    FILE *file = fopen(path, "r+b");
    int byte, failed;
    if (!file) { fprintf(stderr, "cannot open input %s\n", path); return 1; }
    failed = fseek(file, 18855L, SEEK_SET) != 0;
    byte = failed ? EOF : fgetc(file);
    failed = failed || byte == EOF || fseek(file, 18855L, SEEK_SET) != 0 ||
             fputc(byte ^ 0xff, file) == EOF;
    if (fclose(file) != 0) failed = 1;
    if (failed) fprintf(stderr, "cannot flip coefficient byte in %s\n", path);
    return failed;
}

/* read_lines() in the former Python mutator normalized CRLF and split the
 * reference into 121 lines without including the trailing empty line. */
static int fixture_lines(const char *path, unsigned char **storage,
                         line_view lines[FIXTURE_ROWS]) {
    unsigned char *raw, *data;
    size_t n, size, i, start = 0, count = 0;
    if (load_file(path, &raw, &size)) return 1;
    if (normalize_lf(raw, size, &data, &n)) { free(raw); return 1; }
    free(raw);
    for (i = 0; i <= n; ++i) {
        if (i != n && data[i] != '\n') continue;
        if (i == n && start == n) break;
        if (count >= FIXTURE_ROWS) break;
        lines[count].bytes = data + start;
        lines[count++].length = i - start;
        start = i + 1;
    }
    if (count != FIXTURE_ROWS || start < n) {
        fprintf(stderr, "expected 121 fixture lines: %s\n", path);
        free(data);
        return 1;
    }
    *storage = data;
    return 0;
}

static int write_lines(const char *path, const line_view lines[FIXTURE_ROWS], int identities) {
    size_t i, capacity = 0, written = 0;
    unsigned char *output;
    int rc;
    for (i = 0; i < FIXTURE_ROWS; ++i) {
        if (lines[i].length >= SIZE_MAX - capacity) return 1;
        capacity += lines[i].length + 1;
    }
    output = malloc(capacity);
    if (!output) return 1;
    for (i = 0; i < FIXTURE_ROWS; ++i) {
        if (!identities) {
            memcpy(output + written, lines[i].bytes, lines[i].length);
            written += lines[i].length;
        } else {
            size_t pos = 0, token_index = 0;
            while (pos < lines[i].length) {
                size_t begin;
                while (pos < lines[i].length && isspace(lines[i].bytes[pos])) ++pos;
                begin = pos;
                while (pos < lines[i].length && !isspace(lines[i].bytes[pos])) ++pos;
                if (begin == pos) break;
                if (token_index >= 2 && token_index <= 5) {
                    if (token_index > 2) output[written++] = ' ';
                    memcpy(output + written, lines[i].bytes + begin, pos - begin);
                    written += pos - begin;
                }
                ++token_index;
            }
            if (token_index < 6) {
                fprintf(stderr, "fixture line %zu has too few columns\n", i + 1);
                free(output);
                return 1;
            }
        }
        output[written++] = '\n';
    }
    rc = save_file(path, output, written);
    free(output);
    return rc;
}

static int duplicate_rows(const char *text_in, const char *text_out, const char *rows_out) {
    line_view original[FIXTURE_ROWS], duplicate[FIXTURE_ROWS];
    unsigned char *storage;
    size_t i;
    int rc;
    if (fixture_lines(text_in, &storage, original)) return 1;
    for (i = 0; i < FIXTURE_ROWS - 1; ++i) duplicate[i] = original[0];
    duplicate[FIXTURE_ROWS - 1] = original[FIXTURE_ROWS - 1];
    rc = write_lines(text_out, duplicate, 0);
    if (!rc) rc = write_lines(rows_out, duplicate, 1);
    free(storage);
    return rc;
}

static int replace_once(line_view line, const char *before, const char *after) {
    size_t i, length = strlen(before), matches = 0;
    if (length != strlen(after)) return 1;
    for (i = 0; i + length <= line.length; ++i) {
        if (memcmp(line.bytes + i, before, length) == 0) {
            memcpy(line.bytes + i, after, length);
            ++matches;
            i += length - 1;
        }
    }
    if (matches != 1) fprintf(stderr, "expected one fixture field match, found %zu\n", matches);
    return matches != 1;
}

static int replace_row(const char *text_in, const char *text_out, const char *rows_out,
                       size_t row, const char *before, const char *after) {
    line_view lines[FIXTURE_ROWS];
    unsigned char *storage;
    int rc;
    if (fixture_lines(text_in, &storage, lines)) return 1;
    rc = replace_once(lines[row], before, after);
    if (!rc) rc = write_lines(text_out, lines, 0);
    if (!rc && rows_out) rc = write_lines(rows_out, lines, 1);
    free(storage);
    return rc;
}

static int crlf_one(const char *input, const char *output) {
    unsigned char *raw, *normal, *crlf;
    size_t length, size, extra = 0, i, written = 0;
    int rc;
    if (load_file(input, &raw, &length)) return 1;
    rc = normalize_lf(raw, length, &normal, &size);
    free(raw);
    if (rc) return rc;
    for (i = 0; i < size; ++i) if (normal[i] == '\n') ++extra;
    if (extra > SIZE_MAX - size) { free(normal); return 1; }
    crlf = malloc(size + extra ? size + extra : 1);
    if (!crlf) { free(normal); return 1; }
    for (i = 0; i < size; ++i) {
        if (normal[i] == '\n') crlf[written++] = '\r';
        crlf[written++] = normal[i];
    }
    rc = save_file(output, crlf, written);
    free(crlf);
    free(normal);
    return rc;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "canon-copy") == 0)
        return canon_copy(argv[2], argv[3]);
    if (argc == 3 && strcmp(argv[1], "flip") == 0)
        return flip(argv[2]);
    if (argc == 6 && strcmp(argv[1], "dup") == 0)
        return duplicate_rows(argv[2], argv[3], argv[5]);
    if (argc == 6 && strcmp(argv[1], "retarget") == 0)
        return replace_row(argv[2], argv[3], argv[5], 60,
            "2453371.5  1  9  6", "2453371.5  2  9  6");
    if (argc == 4 && strcmp(argv[1], "recoord") == 0)
        return replace_row(argv[2], argv[3], NULL, 89,
            "-0.02293917668434295246", "-0.02293927668434295246");
    if (argc == 6 && strcmp(argv[1], "crlf") == 0) {
        if (crlf_one(argv[2], argv[3])) return 1;
        return crlf_one(argv[4], argv[5]);
    }
    fputs("invalid fixture tool command or argument count\n", stderr);
    return 2;
}
