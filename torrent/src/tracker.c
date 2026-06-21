#include "common.h"

typedef struct PeerNode {
    PeerInfo info;
    struct PeerNode *next;
} PeerNode;

static PeerNode *g_peers = NULL;

static void trim_newline(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[--len] = '\0';
    }
}

static PeerNode *find_peer(const char *ip, int port, const char *filename)
{
    for (PeerNode *cur = g_peers; cur != NULL; cur = cur->next) {
        if (strcmp(cur->info.ip, ip) == 0 && cur->info.port == port &&
            strcmp(cur->info.filename, filename) == 0) {
            return cur;
        }
    }
    return NULL;
}

static int handle_register(int client_sock, const char *peer_ip, char *line)
{
    char cmd[32];
    PeerInfo info;

    memset(&info, 0, sizeof(info));
    if (sscanf(line, "%31s %d %255s %d %511s",
               cmd, &info.port, info.filename, &info.chunk_count, info.chunks) != 5) {
        send_line(client_sock, "ERR invalid REGISTER format\n");
        return -1;
    }

    if (info.port <= 0 || info.port > 65535 || info.chunk_count <= 0) {
        send_line(client_sock, "ERR invalid peer port or chunk count\n");
        return -1;
    }

    snprintf(info.ip, sizeof(info.ip), "%s", peer_ip);

    PeerNode *node = find_peer(info.ip, info.port, info.filename);
    if (node == NULL) {
        node = (PeerNode *)calloc(1, sizeof(PeerNode));
        if (node == NULL) {
            send_line(client_sock, "ERR tracker memory allocation failed\n");
            return -1;
        }
        node->next = g_peers;
        g_peers = node;
    }
    node->info = info;

    printf("[REGISTER] %s:%d file=%s chunks=%s/%d\n",
           info.ip, info.port, info.filename, info.chunks, info.chunk_count);
    return send_line(client_sock, "OK REGISTERED\n");
}

static int handle_query(int client_sock, char *line)
{
    char cmd[32];
    char filename[MAX_FILENAME_LEN];
    char response[MAX_LINE_LEN];
    int count = 0;

    if (sscanf(line, "%31s %255s", cmd, filename) != 2) {
        send_line(client_sock, "ERR invalid QUERY format\n");
        return -1;
    }

    for (PeerNode *cur = g_peers; cur != NULL; cur = cur->next) {
        if (strcmp(cur->info.filename, filename) == 0) count++;
    }

    snprintf(response, sizeof(response), "PEERS %d\n", count);
    if (send_line(client_sock, response) < 0) return -1;

    for (PeerNode *cur = g_peers; cur != NULL; cur = cur->next) {
        PeerInfo *info = &cur->info;
        if (strcmp(info->filename, filename) != 0) continue;
        snprintf(response, sizeof(response), "%s %d %s %d %s\n",
                 info->ip, info->port, info->filename, info->chunk_count, info->chunks);
        if (send_line(client_sock, response) < 0) return -1;
    }

    printf("[QUERY] file=%s matched_peers=%d\n", filename, count);
    return send_line(client_sock, "END\n");
}

static void handle_client(int client_sock, const char *peer_ip)
{
    char line[MAX_LINE_LEN];
    ssize_t n = recv_line(client_sock, line, sizeof(line));
    if (n <= 0) return;

    trim_newline(line);
    if (strncmp(line, "REGISTER ", 9) == 0) {
        handle_register(client_sock, peer_ip, line);
    } else if (strncmp(line, "QUERY ", 6) == 0) {
        handle_query(client_sock, line);
    } else {
        send_line(client_sock, "ERR unknown command\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <tracker_port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
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

    printf("[INFO] tracker started. listening on port %d...\n", port);
    fflush(stdout);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept() failed");
            continue;
        }

        char peer_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, peer_ip, sizeof(peer_ip));
        handle_client(client_sock, peer_ip);
        close(client_sock);
    }
}
