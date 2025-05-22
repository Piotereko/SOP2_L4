#define _GNU_SOURCE
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"  // your functions here or paste above

#include <stdint.h>
#include <arpa/inet.h>

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *hostname = argv[1];
    char *port = argv[2];

    pid_t pid = getpid();
    printf("PID=%d\n", pid);

    int sockfd = connect_tcp_socket(hostname, port);
    if (sockfd < 0)
        exit(EXIT_FAILURE);

    char pid_str[21];
    snprintf(pid_str, sizeof(pid_str), "%d\n", pid);

    if (bulk_write(sockfd, pid_str, strlen(pid_str)) != (ssize_t)strlen(pid_str))
    {
        perror("write");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int16_t net_sum;
    ssize_t n = bulk_read(sockfd, (char *)&net_sum, sizeof(net_sum));
    if (n != sizeof(net_sum))
    {
        fprintf(stderr, "Failed to read sum from server\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int16_t sum = ntohs(net_sum);
    printf("SUM=%d\n", sum);

    close(sockfd);
    return 0;
}
