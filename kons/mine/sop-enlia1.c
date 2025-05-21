#include "common.h"

#define MAX_CLIENTS 4
#define MAX_USERNAME_LENGTH 32
#define MAX_MESSAGE_SIZE 64
#define MAX_EVENTS (MAX_CLIENTS + 1)

void usage(char* program_name)
{
    fprintf(stderr, "Usage: \n");

    fprintf(stderr, "\t%s", program_name);
    set_color(2, SOP_PINK);
    fprintf(stderr, " port\n");

    fprintf(stderr, "\t  port");
    reset_color(2);
    fprintf(stderr, " - the port on which the server will run\n");

    exit(EXIT_FAILURE);
}



int main(int argc, char** argv) {
    if (argc != 2) usage(argv[0]);
    uint16_t port = atoi(argv[1]);
    if (port <= 1023 || port >= 65535) usage(argv[0]);

    int server_socket = bind_tcp_socket(port, 16);
    int epoll_ds = epoll_create1(0);
    if (epoll_ds < 0) ERR("epoll_create");

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_ds, EPOLL_CTL_ADD, server_socket, &event) == -1)
        ERR("epoll_ctl");

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    while (1) {
        int ready_fds = epoll_wait(epoll_ds, events, MAX_EVENTS, -1);
        if (ready_fds < 0) ERR("epoll_wait");

        for (int i = 0; i < ready_fds; i++) {
            if (events[i].data.fd == server_socket) {
                // Accept new client
                int client_socket = add_new_client(server_socket);
                if (client_socket >= 0) {
                    printf("Client socket: %d|\n", client_socket);
                    struct epoll_event client_event = {0};
                    client_event.events = EPOLLIN;
                    client_event.data.fd = client_socket;
                    if (epoll_ctl(epoll_ds, EPOLL_CTL_ADD, client_socket, &client_event) == -1)
                        ERR("epoll_ctl: client");
                }
            } else {
                // Client socket is ready to read
                int client_fd = events[i].data.fd;
                char buf[MAX_MESSAGE_SIZE + 1];
                ssize_t total_read = 0;
                while (total_read < MAX_MESSAGE_SIZE) {
                    ssize_t r = read(client_fd, buf + total_read, 1);
                    if (r < 0) ERR("read");
                    if (r == 0) {
                        // Client closed connection
                        close(client_fd);
                        epoll_ctl(epoll_ds, EPOLL_CTL_DEL, client_fd, NULL);
                        break;
                    }
                    if (buf[total_read] == '\n') {
                        total_read++;
                        break;
                    }
                    total_read++;
                }
                if (total_read > 0) {
                    buf[total_read] = '\0';
                    printf("%s", buf);
                    if (write(client_fd, "Hello world\n", 12) < 0) {
                        if (errno != EPIPE) ERR("write");
                        close(client_fd);
                        epoll_ctl(epoll_ds, EPOLL_CTL_DEL, client_fd, NULL);
                    }
                }
            }
        }
    }
    // close sockets when done (unreachable in this example)
    close(server_socket);
    close(epoll_ds);

    return EXIT_SUCCESS;
}

