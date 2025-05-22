#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <sys/epoll.h>
#include <time.h>

#include "common.h"

#define BUF_SIZE 128
#define NUM_CITIES 20

typedef enum { UNKNOWN = 0, GREEK, PERSIAN } owner_t;

volatile sig_atomic_t running = 1;

owner_t city_owner[NUM_CITIES + 1]; // 1-based indexing

void sigint_handler(int signo) {
    (void)signo;
    running = 0;
}

void print_ownership() {
    printf("City ownership:\n");
    for (int i = 1; i <= NUM_CITIES; i++) {
        printf("City %02d: ", i);
        switch (city_owner[i]) {
            case UNKNOWN: printf("Unknown\n"); break;
            case GREEK: printf("Greek\n"); break;
            case PERSIAN: printf("Persian\n"); break;
        }
    }
}

int update_ownership(char prefix, int city) {
    if (city < 1 || city > NUM_CITIES) return -1;
    if (prefix == 'g') {
        city_owner[city] = GREEK;
    } else if (prefix == 'p') {
        city_owner[city] = PERSIAN;
    } else {
        return -1;
    }
    return 0;
}

// Send a message, must be 4 chars
int send_message(int sockfd, const char *msg) {
    if (bulk_write(sockfd, (char *)msg, 4) != 4) {
        perror("bulk_write");
        return -1;
    }
    return 0;
}

// Read and handle one message from server (4 bytes)
int handle_server_msg(int sockfd) {
    char buf[4];
    ssize_t r = bulk_read(sockfd, buf, 4);
    if (r == 0) {
        // Server closed connection
        fprintf(stderr, "Server disconnected\n");
        return -1;
    } else if (r < 0) {
        perror("bulk_read");
        return -1;
    } else if (r != 4) {
        fprintf(stderr, "Incomplete server message\n");
        return -1;
    }
    // Format should be pXX\n or gXX\n
    if ((buf[0] != 'g' && buf[0] != 'p') ||
        !isdigit((unsigned char)buf[1]) ||
        !isdigit((unsigned char)buf[2]) ||
        buf[3] != '\n') {
        fprintf(stderr, "Invalid server message format\n");
        return -1;
    }
    int city = (buf[1] - '0') * 10 + (buf[2] - '0');
    if (update_ownership(buf[0], city) != 0) {
        fprintf(stderr, "Invalid city number in server message\n");
        return -1;
    }
    printf("Server updated: %c%02d\n", buf[0], city);
    return 0;
}

// Read a line from stdin (blocking), trim newline
int read_stdin_line(char *buf, size_t max_len) {
    if (fgets(buf, max_len, stdin) == NULL) return -1;
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_address> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    srand((unsigned)time(NULL));
    signal(SIGINT, sigint_handler);

    // Init ownership to unknown
    for (int i = 1; i <= NUM_CITIES; i++)
        city_owner[i] = UNKNOWN;

    int sockfd = connect_tcp_socket(argv[1], argv[2]);
    if (sockfd < 0) {
        perror("connect_tcp_socket");
        return EXIT_FAILURE;
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(sockfd);
        return EXIT_FAILURE;
    }

    struct epoll_event ev, events[2];
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev) < 0) {
        perror("epoll_ctl stdin");
        close(sockfd);
        close(epoll_fd);
        return EXIT_FAILURE;
    }

    ev.data.fd = sockfd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
        perror("epoll_ctl sockfd");
        close(sockfd);
        close(epoll_fd);
        return EXIT_FAILURE;
    }

    char input_buf[BUF_SIZE];

    printf("Connected to server. Commands:\n"
           "e            - exit\n"
           "m XXX        - send 3 chars + newline\n"
           "t XX         - travel: send gXX or pXX with random letter\n"
           "o            - print ownership\n");

    while (running) {
        int nfds = epoll_wait(epoll_fd, events, 2, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == STDIN_FILENO) {
                // Read command line from stdin
                if (read_stdin_line(input_buf, sizeof(input_buf)) < 0) {
                    running = 0;
                    break;
                }

                if (strlen(input_buf) == 0) continue;

                // Parse commands
                if (input_buf[0] == 'e' && strlen(input_buf) == 1) {
                    running = 0;
                    break;
                } else if (input_buf[0] == 'm' && input_buf[1] == ' ' && strlen(input_buf) == 5) {
                    // m XXX (3 chars)
                    char msg[4];
                    memcpy(msg, input_buf + 2, 3);
                    msg[3] = '\n';

                    if (send_message(sockfd, msg) != 0) {
                        running = 0;
                        break;
                    }
                } else if (input_buf[0] == 't' && input_buf[1] == ' ' && strlen(input_buf) == 5) {
                    // t XX travel
                    char *num_str = input_buf + 2;
                    if (!isdigit((unsigned char)num_str[0]) || !isdigit((unsigned char)num_str[1])) {
                        fprintf(stderr, "Invalid city number format\n");
                        continue;
                    }
                    int city = (num_str[0] - '0') * 10 + (num_str[1] - '0');
                    if (city < 1 || city > NUM_CITIES) {
                        fprintf(stderr, "City number out of range\n");
                        continue;
                    }
                    char prefix = (rand() % 2) ? 'g' : 'p';
                    char msg[5];
                    snprintf(msg, sizeof(msg), "%c%02d\n", prefix, city);

                    if (send_message(sockfd, msg) != 0) {
                        running = 0;
                        break;
                    }

                    // Update local ownership immediately
                    update_ownership(prefix, city);
                } else if (input_buf[0] == 'o' && strlen(input_buf) == 1) {
                    print_ownership();
                } else {
                    fprintf(stderr, "Unknown command or wrong format\n");
                }

            } else if (fd == sockfd) {
                // Server sent a message
                if (handle_server_msg(sockfd) < 0) {
                    running = 0;
                    break;
                }
            }
        }
    }

    printf("Exiting...\n");
    print_ownership();

    close(sockfd);
    close(epoll_fd);

    return 0;
}
