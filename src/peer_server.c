#include "common.h"

typedef struct ServerConfig {
    char chunk_dir[MAX_PATH_LEN];
} ServerConfig;

static int send_chunk(int client_sock, const char *chunk_dir, const char *filename, int chunk_index)
{
    char path[MAX_PATH_LEN];
    char header[MAX_LINE_LEN];
    char buffer[BUFFER_SIZE];

    snprintf(path, sizeof(path), "%s/%s.part%04d", chunk_dir, filename, chunk_index);

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        send_line(client_sock, "ERR chunk not found\n");
        fprintf(stderr, "[ERROR] chunk not found: %s\n", path);
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        send_line(client_sock, "ERR chunk stat failed\n");
        return -1;
    }
    long end = ftell(fp);
    if (end < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        send_line(client_sock, "ERR chunk stat failed\n");
        return -1;
    }

    uint64_t size = (uint64_t)end;
    snprintf(header, sizeof(header), "OK %llu\n", (unsigned long long)size);
    if (send_line(client_sock, header) < 0) {
        fclose(fp);
        return -1;
    }

    uint64_t sent = 0;
    while (sent < size) {
        size_t n = fread(buffer, 1, sizeof(buffer), fp);
        if (n == 0) {
            if (ferror(fp)) perror("[ERROR] fread failed");
            fclose(fp);
            return -1;
        }
        if (send_all(client_sock, buffer, n) != (ssize_t)n) {
            fprintf(stderr, "[ERROR] send chunk data failed\n");
            fclose(fp);
            return -1;
        }
        sent += n;
    }

    fclose(fp);
    printf("[UPLOAD] %s chunk=%d size=%llu\n",
           filename, chunk_index, (unsigned long long)size);
    fflush(stdout);
    return 0;
}

static void *client_thread(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg);

    extern ServerConfig g_config;
    char line[MAX_LINE_LEN];
    char cmd[32];
    char filename[MAX_FILENAME_LEN];
    int chunk_index;

    if (recv_line(client_sock, line, sizeof(line)) <= 0) {
        close(client_sock);
        return NULL;
    }

    if (sscanf(line, "%31s %255s %d", cmd, filename, &chunk_index) != 3 ||
        strcmp(cmd, "GET") != 0 || chunk_index < 0) {
        send_line(client_sock, "ERR invalid GET format\n");
    } else {
        send_chunk(client_sock, g_config.chunk_dir, filename, chunk_index);
    }

    close(client_sock);
    return NULL;
}

ServerConfig g_config;

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <chunk_dir>\n", argv[0]);
        fprintf(stderr, "ex   : %s 9001 ./peer1_chunks\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    snprintf(g_config.chunk_dir, sizeof(g_config.chunk_dir), "%s", argv[2]);

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        perror("socket() failed");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        close(listen_sock);
        return 1;
    }

    if (listen(listen_sock, 16) < 0) {
        perror("listen() failed");
        close(listen_sock);
        return 1;
    }

    printf("[INFO] peer server started. port=%d chunk_dir=%s\n", port, g_config.chunk_dir);
    fflush(stdout);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept() failed");
            continue;
        }

        int *sock_arg = (int *)malloc(sizeof(int));
        if (sock_arg == NULL) {
            close(client_sock);
            continue;
        }
        *sock_arg = client_sock;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, sock_arg) != 0) {
            perror("pthread_create failed");
            close(client_sock);
            free(sock_arg);
            continue;
        }
        pthread_detach(tid);
    }
}
