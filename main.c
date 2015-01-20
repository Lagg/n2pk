/* Reference implementation of formal n2pk mapping.
 * TODO: Until otherwise noted this is pretty dirty and
 * needs optimizing once I figure out a few more things in the format.
 * It doesn't leak or anything but it lacks fuzz testing and has
 * a few peppered magic numbers and lacks error checks
 *
 * Copyright(C) 2015+, Anthony Garcia <anthony@lagg.me>
 * Distributed under the ISC License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <unistd.h>

/* Some of the strings below
 * are unicode, but for now they'll be read
 * in as is
 */

const char *N2PK_SIGNATURE = "N\0e\0o\0c\0o\0r\0e\0 \0P\0a\0c\0k\0a\0g\0e\0\0\0";

/* Misaligned :( */
typedef struct {
    uint32_t magic;
    uint8_t signature[32];
    uint64_t body_size;
} n2pk_header;

typedef struct {
    uint32_t hash;
    uint32_t filename_size;
    char *filename;
    uint8_t reserved[2];
    uint64_t file_offset;
    uint64_t file_size;
} n2pk_file_entry;

typedef struct {
    uint32_t entry_count;
    n2pk_file_entry **entries;
} n2pk_footer;

/* Yes this is filthy, but frankly so is trying to handle
 * the wide characters formally. Why do so if there's no
 * purpose. My personal rule of thumb is to avoid wchar
 * facilities unless you really, REALLY need them.
 *
 * and that's why I still have hair left. Yes there
 * may be a redundant loop proceeding but when I know there
 * won't be UTF-32 or some other horrific thing lurking I'll
 * fix it.
 */
char *n2pk_to_ascii(char *widestring, size_t length) {
    for (unsigned int i = 0; i < length; ++i) {
        if (widestring[i] == 0) {
            unsigned int j;
            char nuchar = 0;

            for (j = i;  j < length; ++j) {
                nuchar = widestring[j];

                if (nuchar != 0) {
                    break;
                }
            }

            if (nuchar != 0) {
                widestring[i] = nuchar;
                widestring[j] = 0;
            }
        }
    }

    return widestring;
}

n2pk_header *n2pk_read_header(FILE *stream) {
    n2pk_header *hdr;

    hdr = (n2pk_header *)calloc(1, sizeof(n2pk_header));

    fread(&hdr->magic, 4, 1, stream);
    fread(hdr->signature, sizeof(hdr->signature), 1, stream);
    fread(&hdr->body_size, 8, 1, stream);

    if (memcmp(hdr->signature, N2PK_SIGNATURE, 32) != 0) {
        free(hdr);
        return NULL;
    } else {
        return hdr;
    }
}

n2pk_file_entry *n2pk_read_file_entry(FILE *stream) {
    n2pk_file_entry *entry = NULL;

    entry = (n2pk_file_entry *)calloc(1, sizeof(n2pk_file_entry));

    fread(&entry->hash, 4, 1, stream);
    fread(&entry->filename_size, 4, 1, stream);

    entry->filename = (char *)malloc(entry->filename_size * 2);

    fread(entry->filename, entry->filename_size * 2,  1, stream);

    fread(&entry->reserved, 2, 1, stream);
    fread(&entry->file_offset, 8, 1, stream);
    fread(&entry->file_size, 8, 1, stream);

    return entry;
}

n2pk_footer *n2pk_read_footer(FILE *stream, n2pk_header *header) {
    n2pk_footer *footer;

    footer = (n2pk_footer *)calloc(1, sizeof(n2pk_footer));

    fseek(stream, header->body_size + 44, SEEK_SET);

    fread(&footer->entry_count, 4, 1, stream);

    footer->entries = (n2pk_file_entry **)calloc(footer->entry_count, sizeof(n2pk_file_entry *));

    for (unsigned int i = 0; i < footer->entry_count; ++i) {
        footer->entries[i] = n2pk_read_file_entry(stream);
    }

    return footer;
}

void n2pk_free_footer(n2pk_footer *footer) {
    for (unsigned int i = 0; i < footer->entry_count; ++i) {
        n2pk_file_entry *entry = footer->entries[i];

        free(entry->filename);
        free(entry);
    }

    free(footer->entries);
    free(footer);
}

void n2pk_print_entries(n2pk_footer *footer) {
    for (unsigned int i = 0; i < footer->entry_count; ++i) {
        n2pk_file_entry *entry = footer->entries[i];

        printf("%08X\t%lu\t%lu\t%s\n", entry->hash, entry->file_offset + 44, entry->file_size, n2pk_to_ascii(entry->filename, entry->filename_size * 2));
    }
}

void n2pk_extract_entries(const char *outdir, FILE *stream, n2pk_footer *footer) {
    if (mkdir(outdir, S_IRWXU) != 0 && errno != EEXIST) {
        perror("Couldn't create output directory");
        return;
    }

    for (unsigned int i = 0; i < footer->entry_count; ++i) {
        n2pk_file_entry *entry = footer->entries[i];
        char *filename = NULL;
        char *payload = NULL;
        FILE *outfile = NULL;
        size_t relative_path_size = 0;

        relative_path_size = strlen(outdir) + (entry->filename_size * 2) + 2;
        payload = (char *)malloc(entry->file_size);

        /* Allocate enough to prepend the dir name and path sep token */
        filename = (char *)malloc(relative_path_size);

        snprintf(filename, relative_path_size, "%s/%s", outdir, n2pk_to_ascii(entry->filename, entry->filename_size * 2));

        fseek(stream, entry->file_offset + 44, SEEK_SET);
        fread(payload, 1, entry->file_size, stream);

        outfile = fopen(filename, "wb");

        fwrite(payload, 1, entry->file_size, outfile);

        free(payload);
        fclose(outfile);
        free(filename);
    }
}

int main(int argc, char **argv) {
    n2pk_header *hdr = NULL;
    n2pk_footer *ftr = NULL;
    FILE *stream = NULL;
    char *input_filename = NULL;
    char *output_dir = NULL;
    char *extension_in_inputfn = NULL;

    int opt = 0;
    int operation = 0;
    const char * const usage = "-f <filename|stdin> | -l | -h | -x";

    while ((opt = getopt(argc, argv, "f:lhx")) != -1) {
        switch (opt) {
            case 'f':
                input_filename = (char *)malloc(strlen(optarg) + 1);
                strcpy(input_filename, optarg);
                break;
            case 'l':
            case 'x':
                operation = opt;
                break;
            case 'h':
                fprintf(stderr, "%s %s\n\n-f: file to read from. Pass 'stdin' for stdin.\n-h: help\n-l: list file entries\n-x: extract files\n", argv[0], usage);
                return EXIT_SUCCESS;
                break;
            default:
                fprintf(stderr, "Usage: %s %s\n", argv[0], usage);
                return EXIT_FAILURE;
                break;
        }
    }

    if (!input_filename) {
        fprintf(stderr, "Archive filename required\n");
        return EXIT_FAILURE;
    }

    if (strcmp(input_filename, "stdin") != 0) {
        stream = fopen(input_filename, "rb");
    } else {
        stream = stdin;
    }

    if (stream == NULL) {
        perror("Error opening stream for reading");
        return EXIT_FAILURE;
    }

    hdr = n2pk_read_header(stream);

    if (!hdr) {
        fprintf(stderr, "Error validating archive header\n");
        return EXIT_FAILURE;
    }

    ftr = n2pk_read_footer(stream, hdr);

    if (operation == 'l') {
        n2pk_print_entries(ftr);
    } else if (operation == 'x') {
        output_dir = (char *)calloc(strlen(input_filename) + 1, 1);
        strcpy(output_dir, input_filename);
        extension_in_inputfn = strstr(output_dir, ".n2pk");

        if (extension_in_inputfn) {
            *extension_in_inputfn = 0;
        }

        n2pk_extract_entries(output_dir, stream, ftr);

        free(output_dir);
    }

    fclose(stream);
    free(input_filename);
    free(hdr);
    n2pk_free_footer(ftr);

    return EXIT_SUCCESS;
}
