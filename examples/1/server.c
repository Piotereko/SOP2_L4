#define _GNU_SOURCE
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"  // <-- assume your functions are here or just paste the functions above in this file

static volatile sig_atomic_t do_exit = 0;
static int listen_fd = -1;
static int16_t highest_sum = 0;

void sigint_handler(int signo)
{
    (void)signo;
    do_exit = 1;
    if (listen_fd >= 0)
        close(listen_fd);
}

int16_t sum_digits(const char *str)
{
    int16_t sum = 0;
    for (; *str; str++)
    {
        if (*str >= '0' && *str <= '9')
            sum += *str - '0';
    }
    return sum;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "Invalid port number\n");
        exit(EXIT_FAILURE);
    }

    listen_fd = bind_tcp_socket((uint16_t)port, 5);
    if (listen_fd < 0)
        exit(EXIT_FAILURE);

    printf("Server listening on port %d\n", port);

    while (!do_exit)
    {
        int client_fd = add_new_client(listen_fd);
        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            perror("accept");
            break;
        }

        char pid_buf[21];
size_t pos = 0;
while (pos < sizeof(pid_buf) - 1) {
    char c;
    ssize_t r = read(client_fd, &c, 1);
    if (r < 0) {
        if (errno == EINTR) continue;
        break;
    }
    if (r == 0) break; // EOF
    pid_buf[pos++] = c;
    if (c == '\n') break;
}
pid_buf[pos] = '\0';
// Strip trailing newline if present
if (pos > 0 && pid_buf[pos-1] == '\n')
    pid_buf[pos-1] = '\0';

        int16_t sum = sum_digits(pid_buf);
        if (sum > highest_sum)
            highest_sum = sum;

        int16_t net_sum = htons(sum);
        bulk_write(client_fd, (char *)&net_sum, sizeof(net_sum));

        close(client_fd);
    }

    printf("HIGH SUM=%d\n", highest_sum);

    if (listen_fd >= 0)
        close(listen_fd);

    return 0;
}
