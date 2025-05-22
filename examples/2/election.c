#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#define MAX_ELECTORS 7
#define MAX_EVENTS 10
#define BUF_SIZE 128

struct elector {
    int id;                 // 1..7
    const char *state_name;
    int connected;          // 0/1
    int sockfd;             // client socket fd, or -1
    int vote;               // 0 if none, else 1..3
    int identified;         // 0/1
};

struct elector electors[MAX_ELECTORS] = {
    {1, "Mainz", 0, -1, 0, 0},
    {2, "Trier", 0, -1, 0, 0},
    {3, "Cologne", 0, -1, 0, 0},
    {4, "Bohemia", 0, -1, 0, 0},
    {5, "Palantinate", 0, -1, 0, 0},
    {6, "Saxony", 0, -1, 0, 0},
    {7, "Brandenburg", 0, -1, 0, 0},
};

volatile sig_atomic_t running = 1;
pthread_mutex_t votes_mutex = PTHREAD_MUTEX_INITIALIZER;

// Helper to get elector by sockfd
struct elector *get_elector_by_fd(int fd) {
    for (int i=0; i<MAX_ELECTORS; i++)
        if (electors[i].sockfd == fd)
            return &electors[i];
    return NULL;
}

// Helper to get elector by id
struct elector *get_elector_by_id(int id) {
    if (id < 1 || id > MAX_ELECTORS)
        return NULL;
    return &electors[id-1];
}

void sigint_handler(int signo) {
    (void)signo;
    running = 0;
}

void print_results() {
    pthread_mutex_lock(&votes_mutex);
    int counts[4] = {0}; // index 1..3 for candidates
    for (int i=0; i<MAX_ELECTORS; i++) {
        if (electors[i].vote >= 1 && electors[i].vote <= 3)
            counts[electors[i].vote]++;
    }
    printf("Final results:\n");
    for (int c=1; c<=3; c++)
        printf("Candidate %d: %d votes\n", c, counts[c]);
    pthread_mutex_unlock(&votes_mutex);
}

// UDP thread and other code omitted for brevity

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s tcp_port [udp_port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigint_handler);

    int tcp_port = atoi(argv[1]);
    if (tcp_port <= 0) {
        fprintf(stderr, "Invalid TCP port\n");
        exit(EXIT_FAILURE);
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(tcp_port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 10) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1"); exit(EXIT_FAILURE);
    }

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl"); exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", tcp_port);

    while (running) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait"); break;
        }

        for (int i=0; i<nfds; i++) {
            if (events[i].data.fd == listen_fd) {
                // New client connection
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
                if (client_fd < 0) {
                    perror("accept");
                    continue;
                }

                // Add client to epoll
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                    perror("epoll_ctl ADD client");
                    close(client_fd);
                    continue;
                }
                // Client initially unidentified
                // No greeting yet
            } else {
                // Client socket has data
                int client_fd = events[i].data.fd;
                struct elector *el = get_elector_by_fd(client_fd);

                char buf[BUF_SIZE];
                ssize_t r = recv(client_fd, buf, sizeof(buf), 0);
                if (r <= 0) {
                    // Client disconnected or error
                    if (el) {
                        printf("Elector %d (%s) disconnected\n", el->id, el->state_name);
                        pthread_mutex_lock(&votes_mutex);
                        el->connected = 0;
                        el->sockfd = -1;
                        pthread_mutex_unlock(&votes_mutex);
                    }
                    close(client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    continue;
                }

                // Process data
                for (ssize_t j=0; j<r; j++) {
                    char c = buf[j];
                    if (!el) {
                        // Not identified yet, expect one digit [1..7] only once
                        if (c >= '1' && c <= '7') {
                            int id = c - '0';
                            struct elector *candidate = get_elector_by_id(id);
                            pthread_mutex_lock(&votes_mutex);
                            if (candidate->connected) {
                                // Already connected, reject new connection
                                pthread_mutex_unlock(&votes_mutex);
                                printf("Elector %d tried to connect again - disconnecting\n", id);
                                close(client_fd);
                                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            } else {
                                // Mark elector connected
                                candidate->connected = 1;
                                candidate->sockfd = client_fd;
                                candidate->identified = 1;
                                pthread_mutex_unlock(&votes_mutex);
                                el = candidate;
                                char welcome_msg[128];
                                snprintf(welcome_msg, sizeof(welcome_msg),
                                         "Welcome, elector of %s!\n", candidate->state_name);
                                send(client_fd, welcome_msg, strlen(welcome_msg), 0);
                                printf("Elector %d (%s) connected\n", id, candidate->state_name);
                            }
                        } else {
                            // Invalid ID, disconnect
                            close(client_fd);
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        }
                    } else {
                        // Identified elector sending votes [1..3]
                        if (c >= '1' && c <= '3') {
                            int vote = c - '0';
                            pthread_mutex_lock(&votes_mutex);
                            el->vote = vote;
                            pthread_mutex_unlock(&votes_mutex);
                            printf("Elector %d (%s) voted for candidate %d\n",
                                   el->id, el->state_name, vote);
                        } else {
                            // ignore other chars
                        }
                    }
                }
            }
        }
    }

    printf("Shutting down...\n");

    // Close all client sockets
    for (int i=0; i<MAX_ELECTORS; i++) {
        if (electors[i].connected && electors[i].sockfd >= 0) {
            close(electors[i].sockfd);
        }
    }
    close(listen_fd);
    close(epoll_fd);

    print_results();

    return 0;
}
