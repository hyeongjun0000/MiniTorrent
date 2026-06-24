#include "common.h"

int main(int argc, char *argv[])
{
    if (argc != 7) {
        fprintf(stderr, "usage: %s <tracker_ip> <tracker_port> <peer_port> <filename> <chunk_count> <chunks_csv>\n", argv[0]);
        return 1;
    }

    const char *tracker_ip = argv[1];
    int tracker_port = atoi(argv[2]);
    int peer_port = atoi(argv[3]);
    const char *filename = argv[4];
    int chunk_count = atoi(argv[5]);
    const char *chunks = argv[6];

    int sock = connect_tcp(tracker_ip, tracker_port);
    if (sock < 0) return 1;

    char request[MAX_LINE_LEN];
    char response[MAX_LINE_LEN];
    snprintf(request, sizeof(request), "REGISTER %d %s %d %s\n",
             peer_port, filename, chunk_count, chunks);

    if (send_line(sock, request) < 0 || recv_line(sock, response, sizeof(response)) <= 0) {
        fprintf(stderr, "[ERROR] tracker communication failed\n");
        close(sock);
        return 1;
    }

    printf("%s", response);
    close(sock);
    return strncmp(response, "OK ", 3) == 0 ? 0 : 1;
}
