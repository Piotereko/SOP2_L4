#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

ssize_t bulk_read(int fd, char *buf, size_t count) {
    size_t total = 0;
    while (total < count) {
        ssize_t r = read(fd, buf + total, count - total);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break; // EOF
        total += r;
    }
    return total;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int listen_port = atoi(argv[1]);
    if (listen_port <= 0 || listen_port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return EXIT_FAILURE;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    if (listen(listen_fd, 1) < 0) {
        perror("listen");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    printf("Server listening on port %d\n", listen_port);

    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    // First read 1 byte: size of the message (including port and message)
    uint8_t size;
    ssize_t r = bulk_read(client_fd, (char *)&size, 1);
    if (r != 1) {
        fprintf(stderr, "Failed to read size byte\n");
        close(client_fd);
        close(listen_fd);
        return EXIT_FAILURE;
    }

    if (size < 2) {
        fprintf(stderr, "Size must be at least 2 (for port bytes)\n");
        close(client_fd);
        close(listen_fd);
        return EXIT_FAILURE;
    }

    // Read next 'size' bytes: 2 bytes port + message
    char *buffer = malloc(size);
    if (!buffer) {
        perror("malloc");
        close(client_fd);
        close(listen_fd);
        return EXIT_FAILURE;
    }

    r = bulk_read(client_fd, buffer, size);
    if (r != size) {
        fprintf(stderr, "Failed to read full message\n");
        free(buffer);
        close(client_fd);
        close(listen_fd);
        return EXIT_FAILURE;
    }

    // Extract port
    uint16_t port_net;
    memcpy(&port_net, buffer, 2);
    uint16_t port = ntohs(port_net);

    // Extract message
    size_t msg_len = size - 2;
    char *msg = malloc(msg_len + 1);
    if (!msg) {
        perror("malloc");
        free(buffer);
        close(client_fd);
        close(listen_fd);
        return EXIT_FAILURE;
    }
    memcpy(msg, buffer + 2, msg_len);
    msg[msg_len] = '\0'; // null-terminate

    printf("Received port: %u\n", port);
    printf("Received message: %s\n", msg);

    free(buffer);
    free(msg);
    close(client_fd);
    close(listen_fd);

    return 0;
}
