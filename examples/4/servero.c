#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "common.h"  // Your helper header

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_address> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *server = argv[1];
    const char *port = argv[2];

    // Connect TCP socket using helper
    int sockfd = connect_tcp_socket((char *)server, (char *)port);
    if (sockfd < 0) {
        perror("connect_tcp_socket");
        return EXIT_FAILURE;
    }

    char buf[4];
    size_t read_bytes = 0;
    while (read_bytes < 4) {
        ssize_t r = read(STDIN_FILENO, buf + read_bytes, 4 - read_bytes);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("read");
            close(sockfd);
            return EXIT_FAILURE;
        }
        if (r == 0) break; // EOF
        read_bytes += r;
    }

    if (read_bytes != 4) {
        fprintf(stderr, "Expected exactly 4 bytes from stdin, got %zu\n", read_bytes);
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (bulk_write(sockfd, buf, 4) != 4) {
        perror("bulk_write");
        close(sockfd);
        return EXIT_FAILURE;
    }

    close(sockfd);
    return EXIT_SUCCESS;
}
