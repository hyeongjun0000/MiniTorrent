#include "common.h"

ssize_t send_all(int sock, const void *buf, size_t len)
{
    size_t total_sent = 0;
    const char *p = (const char *)buf;

    while (total_sent < len) {
        ssize_t n = send(sock, p + total_sent, len - total_sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        total_sent += (size_t)n;
    }

    return (ssize_t)total_sent;
}

ssize_t recv_all(int sock, void *buf, size_t len)
{
    size_t total_recv = 0;
    char *p = (char *)buf;

    while (total_recv < len) {
        ssize_t n = recv(sock, p + total_recv, len - total_recv, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        total_recv += (size_t)n;
    }

    return (ssize_t)total_recv;
}

int send_line(int sock, const char *line)
{
    size_t len = strlen(line);
    return send_all(sock, line, len) == (ssize_t)len ? 0 : -1;
}

ssize_t recv_line(int sock, char *buf, size_t size)
{
    size_t used = 0;

    if (size == 0) return -1;

    while (used + 1 < size) {
        char c;
        ssize_t n = recv(sock, &c, 1, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;
        if (c == '\r') continue;

        buf[used++] = c;
        if (c == '\n') break;
    }

    buf[used] = '\0';
    if (used == 0) return 0;
    return (ssize_t)used;
}

int connect_tcp(const char *ip, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket() failed");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "[ERROR] invalid IP address: %s\n", ip);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect() failed");
        close(sock);
        return -1;
    }

    return sock;
}

double get_time_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int ensure_dir(const char *dir)
{
    if (mkdir(dir, 0755) < 0 && errno != EEXIST) {
        perror("[ERROR] mkdir failed");
        return -1;
    }
    return 0;
}

int chunk_list_contains(const char *chunks_csv, int chunk_index)
{
    char copy[MAX_CHUNKS_LEN];
    char *token;
    char *saveptr = NULL;

    snprintf(copy, sizeof(copy), "%s", chunks_csv);
    token = strtok_r(copy, ",", &saveptr);
    while (token != NULL) {
        if (atoi(token) == chunk_index) return 1;
        token = strtok_r(NULL, ",", &saveptr);
    }

    return 0;
}

int read_manifest(const char *manifest_path, char *filename, size_t filename_size,
                  uint64_t *filesize, uint64_t *chunk_size,
                  ChunkMeta *chunks, int *chunk_count)
{
    FILE *fp = fopen(manifest_path, "r");
    if (fp == NULL) {
        perror("[ERROR] cannot open manifest");
        return -1;
    }

    char key[64];
    char chunks_label[64];
    unsigned long long file_size_value;
    unsigned long long chunk_size_value;
    int count_value;

    if (fscanf(fp, "%63s %255s", key, filename) != 2 || strcmp(key, "filename") != 0) goto invalid;
    filename[filename_size - 1] = '\0';
    if (fscanf(fp, "%63s %llu", key, &file_size_value) != 2 || strcmp(key, "filesize") != 0) goto invalid;
    if (fscanf(fp, "%63s %llu", key, &chunk_size_value) != 2 || strcmp(key, "chunk_size") != 0) goto invalid;
    if (fscanf(fp, "%63s %d", key, &count_value) != 2 || strcmp(key, "chunk_count") != 0) goto invalid;
    if (count_value <= 0 || count_value > MAX_CHUNKS) goto invalid;
    if (fscanf(fp, "%63s", chunks_label) != 1 || strcmp(chunks_label, "chunks") != 0) goto invalid;

    for (int i = 0; i < count_value; i++) {
        unsigned long long size_value;
        if (fscanf(fp, "%d %llu %1023s", &chunks[i].index, &size_value, chunks[i].path) != 3) goto invalid;
        chunks[i].size = size_value;
        if (chunks[i].index != i) goto invalid;
    }

    fclose(fp);
    *filesize = file_size_value;
    *chunk_size = chunk_size_value;
    *chunk_count = count_value;
    return 0;

invalid:
    fprintf(stderr, "[ERROR] invalid manifest format: %s\n", manifest_path);
    fclose(fp);
    return -1;
}
