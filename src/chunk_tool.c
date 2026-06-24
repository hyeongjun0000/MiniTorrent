#include <errno.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define MAX_PATH_LEN 1024
#define MAX_NAME_LEN 256

typedef struct ChunkEntry {
    int index;
    uint64_t size;
    char path[MAX_PATH_LEN];
} ChunkEntry;

static uint64_t file_size(FILE *fp)
{
    long current = ftell(fp);
    if (current < 0) return 0;
    if (fseek(fp, 0, SEEK_END) != 0) return 0;
    long end = ftell(fp);
    fseek(fp, current, SEEK_SET);
    return end < 0 ? 0 : (uint64_t)end;
}

static const char *base_name(const char *path, char *buf, size_t size)
{
    snprintf(buf, size, "%s", path);
    return basename(buf);
}

static int copy_n_bytes(FILE *in, FILE *out, uint64_t bytes)
{
    char buffer[BUFFER_SIZE];
    uint64_t copied = 0;

    while (copied < bytes) {
        size_t to_read = bytes - copied > BUFFER_SIZE ? BUFFER_SIZE : (size_t)(bytes - copied);
        size_t n = fread(buffer, 1, to_read, in);

        if (n == 0) {
            if (ferror(in)) perror("[ERROR] fread failed");
            return -1;
        }
        if (fwrite(buffer, 1, n, out) != n) {
            perror("[ERROR] fwrite failed");
            return -1;
        }
        copied += n;
    }

    return 0;
}

static int split_file(const char *filepath, uint64_t chunk_size, const char *out_dir)
{
    if (chunk_size == 0) {
        fprintf(stderr, "[ERROR] chunk_size must be greater than 0\n");
        return 1;
    }

    FILE *in = fopen(filepath, "rb");
    if (in == NULL) {
        perror("[ERROR] cannot open input file");
        return 1;
    }

    uint64_t total_size = file_size(in);
    uint64_t chunk_count = (total_size + chunk_size - 1) / chunk_size;
    char name_buf[MAX_PATH_LEN];
    const char *filename = base_name(filepath, name_buf, sizeof(name_buf));

    if (mkdir(out_dir, 0755) < 0 && errno != EEXIST) {
        perror("[ERROR] mkdir failed");
        fclose(in);
        return 1;
    }

    char manifest_path[MAX_PATH_LEN];
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.txt", out_dir);
    FILE *manifest = fopen(manifest_path, "w");
    if (manifest == NULL) {
        perror("[ERROR] cannot create manifest");
        fclose(in);
        return 1;
    }

    fprintf(manifest, "filename %s\n", filename);
    fprintf(manifest, "filesize %llu\n", (unsigned long long)total_size);
    fprintf(manifest, "chunk_size %llu\n", (unsigned long long)chunk_size);
    fprintf(manifest, "chunk_count %llu\n", (unsigned long long)chunk_count);
    fprintf(manifest, "chunks\n");

    printf("[INFO] splitting %s (%llu bytes) into %llu chunks\n",
           filename, (unsigned long long)total_size, (unsigned long long)chunk_count);

    for (uint64_t i = 0; i < chunk_count; i++) {
        uint64_t remaining = total_size - (i * chunk_size);
        uint64_t current_size = remaining < chunk_size ? remaining : chunk_size;
        char chunk_path[MAX_PATH_LEN];

        snprintf(chunk_path, sizeof(chunk_path), "%s/%s.part%04llu",
                 out_dir, filename, (unsigned long long)i);

        FILE *out = fopen(chunk_path, "wb");
        if (out == NULL) {
            perror("[ERROR] cannot create chunk file");
            fclose(manifest);
            fclose(in);
            return 1;
        }

        if (copy_n_bytes(in, out, current_size) < 0) {
            fclose(out);
            fclose(manifest);
            fclose(in);
            return 1;
        }
        fclose(out);

        fprintf(manifest, "%llu %llu %s\n",
                (unsigned long long)i, (unsigned long long)current_size, chunk_path);
        printf("[CHUNK] %04llu size=%llu path=%s\n",
               (unsigned long long)i, (unsigned long long)current_size, chunk_path);
    }

    fclose(manifest);
    fclose(in);
    printf("[DONE] manifest created: %s\n", manifest_path);
    return 0;
}

static int read_manifest_header(FILE *manifest, uint64_t *filesize, uint64_t *chunk_size,
                                int *chunk_count)
{
    char key[64];
    char filename[MAX_NAME_LEN];
    char chunks_label[64];
    unsigned long long file_size_value;
    unsigned long long chunk_size_value;
    int count_value;

    if (fscanf(manifest, "%63s %255s", key, filename) != 2 || strcmp(key, "filename") != 0) {
        return -1;
    }
    if (fscanf(manifest, "%63s %llu", key, &file_size_value) != 2 || strcmp(key, "filesize") != 0) {
        return -1;
    }
    if (fscanf(manifest, "%63s %llu", key, &chunk_size_value) != 2 || strcmp(key, "chunk_size") != 0) {
        return -1;
    }
    if (fscanf(manifest, "%63s %d", key, &count_value) != 2 || strcmp(key, "chunk_count") != 0) {
        return -1;
    }
    if (fscanf(manifest, "%63s", chunks_label) != 1 || strcmp(chunks_label, "chunks") != 0) {
        return -1;
    }

    *filesize = file_size_value;
    *chunk_size = chunk_size_value;
    *chunk_count = count_value;
    return 0;
}

static int merge_file(const char *manifest_path, const char *out_path)
{
    FILE *manifest = fopen(manifest_path, "r");
    if (manifest == NULL) {
        perror("[ERROR] cannot open manifest");
        return 1;
    }

    uint64_t total_size;
    uint64_t chunk_size;
    int chunk_count;
    if (read_manifest_header(manifest, &total_size, &chunk_size, &chunk_count) < 0) {
        fprintf(stderr, "[ERROR] invalid manifest format\n");
        fclose(manifest);
        return 1;
    }

    FILE *out = fopen(out_path, "wb");
    if (out == NULL) {
        perror("[ERROR] cannot create output file");
        fclose(manifest);
        return 1;
    }

    printf("[INFO] merging %d chunks -> %s\n", chunk_count, out_path);

    for (int expected = 0; expected < chunk_count; expected++) {
        ChunkEntry entry;
        unsigned long long size_value;

        if (fscanf(manifest, "%d %llu %1023s",
                   &entry.index, &size_value, entry.path) != 3) {
            fprintf(stderr, "[ERROR] missing chunk entry %d\n", expected);
            fclose(out);
            fclose(manifest);
            return 1;
        }

        entry.size = size_value;
        if (entry.index != expected) {
            fprintf(stderr, "[ERROR] chunk order mismatch: expected %d, got %d\n",
                    expected, entry.index);
            fclose(out);
            fclose(manifest);
            return 1;
        }

        FILE *in = fopen(entry.path, "rb");
        if (in == NULL) {
            perror("[ERROR] cannot open chunk");
            fclose(out);
            fclose(manifest);
            return 1;
        }

        if (copy_n_bytes(in, out, entry.size) < 0) {
            fclose(in);
            fclose(out);
            fclose(manifest);
            return 1;
        }
        fclose(in);

        printf("[MERGE] %04d size=%llu path=%s\n",
               entry.index, (unsigned long long)entry.size, entry.path);
    }

    fclose(out);
    fclose(manifest);

    FILE *check = fopen(out_path, "rb");
    if (check == NULL) {
        perror("[ERROR] cannot verify output file");
        return 1;
    }
    uint64_t merged_size = file_size(check);
    fclose(check);

    if (merged_size != total_size) {
        fprintf(stderr, "[ERROR] merged size mismatch: expected %llu, got %llu\n",
                (unsigned long long)total_size, (unsigned long long)merged_size);
        return 1;
    }

    printf("[DONE] merge complete (%llu bytes, chunk_size=%llu)\n",
           (unsigned long long)merged_size, (unsigned long long)chunk_size);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage:\n");
        fprintf(stderr, "  %s split <filepath> <chunk_size_bytes> <out_dir>\n", argv[0]);
        fprintf(stderr, "  %s merge <manifest_path> <out_file>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "split") == 0) {
        if (argc != 5) {
            fprintf(stderr, "usage: %s split <filepath> <chunk_size_bytes> <out_dir>\n", argv[0]);
            return 1;
        }
        return split_file(argv[2], strtoull(argv[3], NULL, 10), argv[4]);
    }

    if (strcmp(argv[1], "merge") == 0) {
        if (argc != 4) {
            fprintf(stderr, "usage: %s merge <manifest_path> <out_file>\n", argv[0]);
            return 1;
        }
        return merge_file(argv[2], argv[3]);
    }

    fprintf(stderr, "[ERROR] unknown command: %s\n", argv[1]);
    return 1;
}
