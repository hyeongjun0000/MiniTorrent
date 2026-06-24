CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -pthread

TARGETS = tracker peer_server downloader chunk_tool register_peer verify_file
SRCDIR = src

SOURCES = $(SRCDIR)/chunk_tool.c $(SRCDIR)/common.c $(SRCDIR)/common.h
COMMON_SOURCES = $(SRCDIR)/common.c $(SRCDIR)/common.h

all: $(TARGETS)

tracker: $(SRCDIR)/tracker.c $(COMMON_SOURCES)
	$(CC) $(CFLAGS) -o tracker $(SRCDIR)/tracker.c $(SRCDIR)/common.c

peer_server: $(SRCDIR)/peer_server.c $(COMMON_SOURCES)
	$(CC) $(CFLAGS) -o peer_server $(SRCDIR)/peer_server.c $(SRCDIR)/common.c $(LDFLAGS)

downloader: $(SRCDIR)/downloader.c $(COMMON_SOURCES)
	$(CC) $(CFLAGS) -o downloader $(SRCDIR)/downloader.c $(SRCDIR)/common.c $(LDFLAGS)

chunk_tool: $(SRCDIR)/chunk_tool.c
	$(CC) $(CFLAGS) -o chunk_tool $(SRCDIR)/chunk_tool.c

register_peer: $(SRCDIR)/register_peer.c $(COMMON_SOURCES)
	$(CC) $(CFLAGS) -o register_peer $(SRCDIR)/register_peer.c $(SRCDIR)/common.c

verify_file: $(SRCDIR)/verify_file.c $(SRCDIR)/sha256.c $(SRCDIR)/sha256.h
	$(CC) $(CFLAGS) -o verify_file $(SRCDIR)/verify_file.c $(SRCDIR)/sha256.c

clean:
	rm -f $(TARGETS) *.log *.bin *.csv
	rm -rf source_chunks peer*_chunks downloaded_* rebuilt_*

.PHONY: all clean
