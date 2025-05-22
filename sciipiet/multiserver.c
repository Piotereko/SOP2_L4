#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "common.h"  // your provided header with helper functions

#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100  // optional limit if you want

int make_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) ERR("fcntl getfl");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) ERR("fcntl setfl");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        exit(EXIT_FAILURE);
    }

    // Setup listening socket using your helper
    int listen_fd = bind_tcp_socket((uint16_t)port, 128);
    if (listen_fd < 0) {
        fprintf(stderr, "Failed to bind TCP socket\n");
        exit(EXIT_FAILURE);
    }

    // Set listen socket nonblocking for edge-triggered epoll
    make_nonblocking(listen_fd);

    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
        ERR("epoll_create1");

    // Add listen socket to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // edge-triggered read
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0)
        ERR("epoll_ctl ADD listen_fd");

    struct epoll_event events[MAX_EVENTS];

    printf("Server listening on port %d\n", port);

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            ERR("epoll_wait");
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                // Accept all incoming connections (nonblocking, edge-triggered)
                while (1) {
                    int client_fd = add_new_client(listen_fd);
                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break; // no more clients to accept
                        ERR("add_new_client");
                    }

                    make_nonblocking(client_fd);

                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                        perror("epoll_ctl ADD client");
                        close(client_fd);
                        continue;
                    }

                    printf("Accepted client fd %d\n", client_fd);
                }
            } else {
                // Client socket ready to read
                while (1) {
                    char buf[BUFFER_SIZE];
                    ssize_t count = bulk_read(fd, buf, sizeof(buf));
                    if (count < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // No more data for now
                            break;
                        }
                        perror("bulk_read");
                        close(fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        break;
                    } else if (count == 0) {
                        // Client closed connection
                        printf("Client fd %d disconnected\n", fd);
                        close(fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        break;
                    } else {
                        // Successfully read count bytes, print them
                        fwrite(buf, 1, count, stdout);
                        fflush(stdout);
                        // Note: Because it's edge-triggered, loop to drain input!
                    }
                }
            }
        }
    }

    // Cleanup (never reached in this loop)
    close(listen_fd);
    close(epoll_fd);

    return 0;
}
