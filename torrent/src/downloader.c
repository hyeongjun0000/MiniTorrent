#include "common.h"

typedef struct DownloadTask {
    int chunk_index;
    uint64_t expected_size;
    char filename[MAX_FILENAME_LEN];
    char out_dir[MAX_PATH_LEN];
    PeerInfo peer;
    int success;
} DownloadTask;

static int query_tracker(const char *tracker_ip, int tracker_port, const char *filename,
                         PeerInfo *peers, int *peer_count)
{
    int sock = connect_tcp(tracker_ip, tracker_port);
    if (sock < 0) return -1;

    char request[MAX_LINE_LEN];
    char line[MAX_LINE_LEN];
    snprintf(request, sizeof(request), "QUERY %s\n", filename);
    if (send_line(sock, request) < 0 || recv_line(sock, line, sizeof(line)) <= 0) {
        close(sock);
        return -1;
    }

    int count = 0;
    if (sscanf(line, "PEERS %d", &count) != 1 || count < 0 || count > MAX_PEERS) {
        fprintf(stderr, "[ERROR] invalid tracker response: %s", line);
        close(sock);
        return -1;
    }

    *peer_count = 0;
    for (int i = 0; i < count; i++) {
        PeerInfo info;
        if (recv_line(sock, line, sizeof(line)) <= 0) {
            close(sock);
            return -1;
        }
        if (sscanf(line, "%15s %d %255s %d %511s",
                   info.ip, &info.port, info.filename, &info.chunk_count, info.chunks) != 5) {
            fprintf(stderr, "[ERROR] invalid peer entry: %s", line);
            close(sock);
            return -1;
        }
        peers[(*peer_count)++] = info;
    }

    while (recv_line(sock, line, sizeof(line)) > 0) {
        if (strcmp(line, "END\n") == 0) break;
    }

    close(sock);
    return 0;
}

static int choose_peer_for_chunk(PeerInfo *peers, int peer_count, int chunk_index, PeerInfo *selected)
{
    for (int i = 0; i < peer_count; i++) {
        if (chunk_list_contains(peers[i].chunks, chunk_index)) {
            *selected = peers[i];
            return 0;
        }
    }
    return -1;
}

static void *download_chunk_thread(void *arg)
{
    DownloadTask *task = (DownloadTask *)arg;
    char request[MAX_LINE_LEN];
    char header[MAX_LINE_LEN];
    char path[MAX_PATH_LEN];
    char buffer[BUFFER_SIZE];

    printf("[DOWNLOAD] chunk=%d from %s:%d\n",
           task->chunk_index, task->peer.ip, task->peer.port);
    fflush(stdout);

    int sock = connect_tcp(task->peer.ip, task->peer.port);
    if (sock < 0) return NULL;

    snprintf(request, sizeof(request), "GET %s %d\n", task->filename, task->chunk_index);
    if (send_line(sock, request) < 0 || recv_line(sock, header, sizeof(header)) <= 0) {
        close(sock);
        return NULL;
    }

    unsigned long long size_value;
    if (sscanf(header, "OK %llu", &size_value) != 1) {
        fprintf(stderr, "[ERROR] peer returned error for chunk %d: %s",
                task->chunk_index, header);
        close(sock);
        return NULL;
    }

    if ((uint64_t)size_value != task->expected_size) {
        fprintf(stderr, "[ERROR] chunk %d size mismatch: expected %llu, got %llu\n",
                task->chunk_index, (unsigned long long)task->expected_size, size_value);
        close(sock);
        return NULL;
    }

    snprintf(path, sizeof(path), "%s/%s.part%04d", task->out_dir, task->filename, task->chunk_index);
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("[ERROR] cannot create chunk output");
        close(sock);
        return NULL;
    }

    uint64_t received = 0;
    while (received < task->expected_size) {
        size_t to_read = task->expected_size - received > BUFFER_SIZE
                             ? BUFFER_SIZE
                             : (size_t)(task->expected_size - received);
        ssize_t n = recv(sock, buffer, to_read, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("[ERROR] recv failed");
            fclose(fp);
            close(sock);
            return NULL;
        }
        if (n == 0) {
            fprintf(stderr, "[ERROR] connection closed during chunk %d\n", task->chunk_index);
            fclose(fp);
            close(sock);
            return NULL;
        }
        if (fwrite(buffer, 1, (size_t)n, fp) != (size_t)n) {
            perror("[ERROR] fwrite failed");
            fclose(fp);
            close(sock);
            return NULL;
        }
        received += (uint64_t)n;
    }

    fclose(fp);
    close(sock);
    task->success = 1;
    printf("[DONE] chunk=%d saved=%s\n", task->chunk_index, path);
    fflush(stdout);
    return NULL;
}

static int write_download_manifest(const char *out_dir, const char *filename, uint64_t filesize,
                                   uint64_t chunk_size, ChunkMeta *chunks, int chunk_count)
{
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/manifest.txt", out_dir);

    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        perror("[ERROR] cannot write downloaded manifest");
        return -1;
    }

    fprintf(fp, "filename %s\n", filename);
    fprintf(fp, "filesize %llu\n", (unsigned long long)filesize);
    fprintf(fp, "chunk_size %llu\n", (unsigned long long)chunk_size);
    fprintf(fp, "chunk_count %d\n", chunk_count);
    fprintf(fp, "chunks\n");
    for (int i = 0; i < chunk_count; i++) {
        fprintf(fp, "%d %llu %s/%s.part%04d\n",
                chunks[i].index, (unsigned long long)chunks[i].size,
                out_dir, filename, chunks[i].index);
    }

    fclose(fp);
    printf("[DONE] downloaded manifest: %s\n", path);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 6) {
        fprintf(stderr, "usage: %s <tracker_ip> <tracker_port> <manifest_path> <filename> <out_dir>\n", argv[0]);
        fprintf(stderr, "ex   : %s 127.0.0.1 7000 ./manifest.txt movie.mp4 ./downloaded\n", argv[0]);
        return 1;
    }

    const char *tracker_ip = argv[1];
    int tracker_port = atoi(argv[2]);
    const char *manifest_path = argv[3];
    const char *target_filename = argv[4];
    const char *out_dir = argv[5];

    char manifest_filename[MAX_FILENAME_LEN];
    uint64_t filesize;
    uint64_t chunk_size;
    ChunkMeta chunks[MAX_CHUNKS];
    int chunk_count;

    if (read_manifest(manifest_path, manifest_filename, sizeof(manifest_filename),
                      &filesize, &chunk_size, chunks, &chunk_count) < 0) {
        return 1;
    }
    if (strcmp(manifest_filename, target_filename) != 0) {
        fprintf(stderr, "[ERROR] manifest filename is %s, target is %s\n",
                manifest_filename, target_filename);
        return 1;
    }
    if (ensure_dir(out_dir) < 0) return 1;

    PeerInfo peers[MAX_PEERS];
    int peer_count = 0;
    if (query_tracker(tracker_ip, tracker_port, target_filename, peers, &peer_count) < 0) {
        return 1;
    }
    if (peer_count == 0) {
        fprintf(stderr, "[ERROR] no peers found for %s\n", target_filename);
        return 1;
    }

    printf("[INFO] tracker returned %d peers for %s\n", peer_count, target_filename);

    DownloadTask tasks[MAX_CHUNKS];
    pthread_t tids[MAX_CHUNKS];
    double start = get_time_sec();

    for (int i = 0; i < chunk_count; i++) {
        PeerInfo selected;
        if (choose_peer_for_chunk(peers, peer_count, i, &selected) < 0) {
            fprintf(stderr, "[ERROR] no peer has chunk %d\n", i);
            return 1;
        }

        memset(&tasks[i], 0, sizeof(tasks[i]));
        tasks[i].chunk_index = i;
        tasks[i].expected_size = chunks[i].size;
        snprintf(tasks[i].filename, sizeof(tasks[i].filename), "%s", target_filename);
        snprintf(tasks[i].out_dir, sizeof(tasks[i].out_dir), "%s", out_dir);
        tasks[i].peer = selected;

        if (pthread_create(&tids[i], NULL, download_chunk_thread, &tasks[i]) != 0) {
            perror("pthread_create failed");
            return 1;
        }
    }

    int success_count = 0;
    for (int i = 0; i < chunk_count; i++) {
        pthread_join(tids[i], NULL);
        if (tasks[i].success) success_count++;
    }

    double elapsed = get_time_sec() - start;
    double throughput = (double)filesize / 1024.0 / 1024.0 / (elapsed > 0 ? elapsed : 0.0001);
    printf("[RESULT] chunks=%d/%d elapsed=%.3fs throughput=%.2f MB/s\n",
           success_count, chunk_count, elapsed, throughput);

    if (success_count != chunk_count) {
        fprintf(stderr, "[ERROR] download incomplete\n");
        return 1;
    }

    return write_download_manifest(out_dir, target_filename, filesize, chunk_size, chunks, chunk_count) == 0 ? 0 : 1;
}
