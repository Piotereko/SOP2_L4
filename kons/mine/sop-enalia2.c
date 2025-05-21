#include "common.h"

#define MAX_CLIENTS 4
#define MAX_USERNAME_LENGTH 32
#define MAX_MESSAGE_SIZE 64
#define MAX_EVENTS (MAX_CLIENTS + 1)

typedef struct user_context
{
    char username[MAX_USERNAME_LENGTH];
    char buf[MAX_MESSAGE_SIZE + 1]; // +1 for null terminator
    int offset;
    int user_fd;

} user_context;

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

void block_sigpipe()
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
}

void get_and_check_args(int argc, char** argv, uint16_t* port)
{
    if (argc != 2)
    {
        usage(argv[0]);
    }
    *port = atoi(argv[1]);

    if (*port <= 1023 || *port >= 65535)
    {
        usage(argv[0]);
    }
}

void decline_new_user(int fd)
{
    if (write(fd, "Server is full\n", 15) < 0)
    {
        if (errno != EPIPE)
        {
            ERR("write");
        }
    }
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
}

void add_new_user_to_list(int client_socket, int epoll_ds, user_context* client_list, int* current_connections_number)
{
    user_context new_user_context = {
        .offset = 0,
        .user_fd = client_socket,
    };

    client_list[(*current_connections_number)] = new_user_context;
    (*current_connections_number)++;

    printf("[%d] connected\n", client_socket);

    // Notify other clients about new user logging in
    const char *login_msg = "User logging in...\n";
    for (int i = 0; i < (*current_connections_number) - 1; i++)
    {
        int other_fd = client_list[i].user_fd;
        if (write(other_fd, login_msg, strlen(login_msg)) < 0)
        {
            if (errno != EPIPE)
                ERR("write");
        }
    }

    // Send welcome message to new client
    const char *welcome = "Please enter your username\n";
    if (write(client_socket, welcome, strlen(welcome)) < 0)
    {
        if (errno != EPIPE)
            ERR("write");
    }

    struct epoll_event user_event;
    user_event.events = EPOLLIN;
    user_event.data.fd = client_socket;
    if (epoll_ctl(epoll_ds, EPOLL_CTL_ADD, client_socket, &user_event) == -1)
    {
        perror("epoll_ctl: client_socket");
        exit(EXIT_FAILURE);
    }
}

// void known_user_handler(user_context *user)
// {
//     char buf[MAX_MESSAGE_SIZE];
//     ssize_t r = read(user->user_fd, buf, sizeof(buf));
//     if (r == 0)
//     {
//         // Client disconnected - ignoring per instructions (no cleanup)
//         return;
//     }
//     if (r < 0)
//     {
//         ERR("read");
//     }

//     for (ssize_t i = 0; i < r; i++)
//     {
//         if (user->offset < MAX_MESSAGE_SIZE)
//         {
//             user->buf[user->offset++] = buf[i];
//         }

//         // Print and reset buffer when newline received or buffer full
//         if (buf[i] == '\n' || user->offset == MAX_MESSAGE_SIZE)
//         {
//             user->buf[user->offset] = '\0'; // null terminate
//             printf("Client %d: %s", user->user_fd, user->buf);
//             user->offset = 0;
//         }
//     }
// }

void known_user_handler(struct epoll_event client_event)
{
    char buf[MAX_MESSAGE_SIZE + 1] = {0};
    int read_chars;
    read_chars = read(client_event.data.fd, buf, MAX_MESSAGE_SIZE);
    if (read_chars == 0)
    {
        if (TEMP_FAILURE_RETRY(close(client_event.data.fd)) < 0)
            ERR("close");
    }

    if (read_chars < 0)
    {
        ERR("read");
    }
    printf("%s\n", buf);
}

int main(int argc, char** argv)
{
    block_sigpipe();

    uint16_t port;
    get_and_check_args(argc, argv, &port);

    int server_socket = bind_tcp_socket(port, 16);

    int new_flags = fcntl(server_socket, F_GETFL) | O_NONBLOCK;
    fcntl(server_socket, F_SETFL, new_flags);

    int epoll_ds;

    if ((epoll_ds = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }

    user_context client_list[MAX_CLIENTS];
    int current_connections_number = 0;

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_ds, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int ready_fds = epoll_wait(epoll_ds, events, MAX_EVENTS, -1);
        if (ready_fds < 0)
        {
            if (errno == EINTR)
                continue;
            ERR("epoll_wait");
        }

        for (int i = 0; i < ready_fds; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                // Accept new user
                int client_socket = add_new_client(server_socket);
                if (client_socket < 0)
                    continue;

                if (current_connections_number >= MAX_CLIENTS)
                {
                    // Server full
                    decline_new_user(client_socket);
                }
                else
                {
                    // Add new user
                    add_new_user_to_list(client_socket, epoll_ds, client_list, &current_connections_number);
                }
            }
            else
            {
                // // Message from existing user
                // int client_fd = events[i].data.fd;
                // // Find user_context by fd
                // user_context *user = NULL;
                // for (int j = 0; j < current_connections_number; j++)
                // {
                //     if (client_list[j].user_fd == client_fd)
                //     {
                //         user = &client_list[j];
                //         break;
                //     }
                // }
                // if (user != NULL)
                // {
                //     known_user_handler(user);
                // }

                known_user_handler(events[i]);
            }
        }
    }

    if (TEMP_FAILURE_RETRY(close(server_socket)) < 0)
        ERR("close");

    return EXIT_SUCCESS;
}
