#ifndef COMMON_H
#define COMMON_H

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define MAX_LINE_LEN 1024
#define MAX_FILENAME_LEN 256
#define MAX_CHUNKS_LEN 512
#define MAX_PATH_LEN 1024
#define MAX_PEERS 64
#define MAX_CHUNKS 1024

typedef struct PeerInfo {
    char ip[INET_ADDRSTRLEN];
    int port;
    char filename[MAX_FILENAME_LEN];
    int chunk_count;
    char chunks[MAX_CHUNKS_LEN];
} PeerInfo;

typedef struct ChunkMeta {
    int index;
    uint64_t size;
    char path[MAX_PATH_LEN];
} ChunkMeta;

ssize_t send_all(int sock, const void *buf, size_t len);
ssize_t recv_all(int sock, void *buf, size_t len);
int send_line(int sock, const char *line);
ssize_t recv_line(int sock, char *buf, size_t size);
int connect_tcp(const char *ip, int port);
double get_time_sec(void);
int ensure_dir(const char *dir);
int chunk_list_contains(const char *chunks_csv, int chunk_index);
int read_manifest(const char *manifest_path, char *filename, size_t filename_size,
                  uint64_t *filesize, uint64_t *chunk_size,
                  ChunkMeta *chunks, int *chunk_count);

#endif
