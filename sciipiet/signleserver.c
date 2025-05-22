#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(sockfd); exit(EXIT_FAILURE);
    }

    if (listen(sockfd, 1) < 0) {
        perror("listen"); close(sockfd); exit(EXIT_FAILURE);
    }

    printf("Server listening on port 8888\n");

    int client_fd = accept(sockfd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept"); close(sockfd); exit(EXIT_FAILURE);
    }

    printf("Client connected!\n");

    char buffer[1024];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Received: %s\n", buffer);
    }

    close(client_fd);
    close(sockfd);

    printf("Server exiting.\n");
    return 0;
}
