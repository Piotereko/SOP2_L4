#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <ctype.h>

#include "common.h"

#define MAX_CLIENTS 4
#define MAX_EVENTS 10
#define BUF_SIZE 16
#define NUM_CITIES 20

// Ownership enum
typedef enum { GREEK, PERSIAN } owner_t;

struct client {
    int fd;
};

static struct client clients[MAX_CLIENTS];

// City ownership, indexed 1..20 (0 unused)
owner_t city_owner[NUM_CITIES + 1];

// Globals
volatile sig_atomic_t running = 1;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t city_mutex = PTHREAD_MUTEX_INITIALIZER;

void sigint_handler(int signo) {
    (void)signo;
    running = 0;
}

void init() {
    for (int i=1; i <= NUM_CITIES; i++)
        city_owner[i] = GREEK;

    for (int i=0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;
}

void print_city_ownership() {
    printf("City ownership:\n");
    for (int i=1; i<=NUM_CITIES; i++) {
        printf("City %02d: %s\n", i,
            city_owner[i] == GREEK ? "Greek" : "Persian");
    }
}

int add_client(int fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i=0; i<MAX_CLIENTS; i++) {
        if (clients[i].fd == -1) {
            clients[i].fd = fd;
            pthread_mutex_unlock(&clients_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1; // no space
}

void remove_client(int fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i=0; i<MAX_CLIENTS; i++) {
        if (clients[i].fd == fd) {
            clients[i].fd = -1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void broadcast_message(int sender_fd, const char *msg, size_t len) {
    pthread_mutex_lock(&clients_mutex);
    for (int i=0; i<MAX_CLIENTS; i++) {
        int cfd = clients[i].fd;
        if (cfd != -1 && cfd != sender_fd) {
            ssize_t w = bulk_write(cfd, (char *)msg, len);
            if (w < 0) {
                if (errno == EPIPE) {
                    // Client disconnected while writing, remove it
                    close(cfd);
                    clients[i].fd = -1;
                }
                // ignore other errors for now
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port\n");
        exit(EXIT_FAILURE);
    }

    init();

    int listen_fd = bind_tcp_socket((uint16_t)port, 10);
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to bind socket\n");
        exit(EXIT_FAILURE);
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl listen_fd");
        close(listen_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    while (running) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        for (int i=0; i<nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                int client_fd = add_new_client(listen_fd);
                if (client_fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        continue;
                    perror("accept");
                    continue;
                }

                if (add_client(client_fd) < 0) {
                    // Max clients reached
                    close(client_fd);
                    continue;
                }

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                    perror("epoll_ctl add client");
                    remove_client(client_fd);
                    close(client_fd);
                    continue;
                }
            } else {
                // Client socket data
                char buf[BUF_SIZE] = {0};
                ssize_t r = bulk_read(fd, buf, 4); // read 4 bytes (pXX\n or gXX\n)
                if (r == 0) {
                    // Client disconnected
                    close(fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    remove_client(fd);
                    continue;
                } else if (r < 0) {
                    perror("bulk_read");
                    continue;
                } else if (r != 4) {
                    // Invalid length, disconnect client
                    close(fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    remove_client(fd);
                    continue;
                }

                // Validate message format
                // buf[0]: 'p' or 'g'
                // buf[1], buf[2]: digits
                // buf[3]: '\n'
                if ((buf[0] != 'p' && buf[0] != 'g') ||
                    !isdigit((unsigned char)buf[1]) || !isdigit((unsigned char)buf[2]) ||
                    buf[3] != '\n') {
                    // Invalid format
                    close(fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    remove_client(fd);
                    continue;
                }

                int city = (buf[1] - '0') * 10 + (buf[2] - '0');
                if (city < 1 || city > NUM_CITIES) {
                    // Invalid city number
                    close(fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    remove_client(fd);
                    continue;
                }

                owner_t new_owner = (buf[0] == 'p') ? PERSIAN : GREEK;

                pthread_mutex_lock(&city_mutex);
                if (city_owner[city] != new_owner) {
                    city_owner[city] = new_owner;
                    pthread_mutex_unlock(&city_mutex);

                    // Broadcast to all other clients
                    broadcast_message(fd, buf, 4);
                } else {
                    pthread_mutex_unlock(&city_mutex);
                    // No ownership change, do nothing
                }
            }
        }
    }

    printf("Shutting down...\n");

    // Close all clients
    pthread_mutex_lock(&clients_mutex);
    for (int i=0; i<MAX_CLIENTS; i++) {
        if (clients[i].fd != -1) {
            close(clients[i].fd);
            clients[i].fd = -1;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(listen_fd);
    close(epoll_fd);

    print_city_ownership();

    return 0;
}
