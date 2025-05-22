#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include "common.h"  // use your helpers like connect_tcp_socket(), bulk_write()

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_address> <server_port> <port_to_send>\n", argv[0]);
        fprintf(stderr, "You will enter the message on stdin (max 255 bytes).\n");
        return EXIT_FAILURE;
    }

    char *server_addr = argv[1];
    char *server_port = argv[2];
    int port_to_send = atoi(argv[3]);
    if (port_to_send <= 0 || port_to_send > 65535) {
        fprintf(stderr, "Invalid port to send\n");
        return EXIT_FAILURE;
    }

    int sockfd = connect_tcp_socket(server_addr, server_port);
    if (sockfd < 0) {
        perror("connect_tcp_socket");
        return EXIT_FAILURE;
    }

    // Read message from stdin, max 255 bytes (because size byte is 1 byte)
    char msg[255];
    ssize_t msg_len = read(STDIN_FILENO, msg, sizeof(msg));
    if (msg_len < 0) {
        perror("read");
        close(sockfd);
        return EXIT_FAILURE;
    }
    // Strip newline if present
    if (msg_len > 0 && msg[msg_len - 1] == '\n') msg_len--;

    if (msg_len > 253) { // because size byte counts 2 port bytes + msg_len <= 255
        fprintf(stderr, "Message too long (max 253 bytes)\n");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // Prepare buffer to send
    // total size = 1 size byte + 2 port bytes + msg_len
    size_t total_len = 1 + 2 + msg_len;
    unsigned char *buffer = malloc(total_len);
    if (!buffer) {
        perror("malloc");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // First byte: size of the rest (port + message)
    buffer[0] = (unsigned char)(2 + msg_len);

    // Next two bytes: port in network byte order
    uint16_t port_net = htons(port_to_send);
    memcpy(buffer + 1, &port_net, 2);

    // Copy message bytes
    memcpy(buffer + 3, msg, msg_len);

    // Send all bytes
    ssize_t written = bulk_write(sockfd, (char *)buffer, total_len);
    if (written != (ssize_t)total_len) {
        perror("bulk_write");
        free(buffer);
        close(sockfd);
        return EXIT_FAILURE;
    }

    free(buffer);
    close(sockfd);

    return EXIT_SUCCESS;
}
