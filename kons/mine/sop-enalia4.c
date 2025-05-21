#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>

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

void send_message(int fd, const char *msg)
{
    if(write(fd,msg,strlen(msg))<0)
    {
        if(errno != EPIPE)
        ERR("write");
    }
}

void send_colored_message(int fd,enum SOP_COLOR color,const char *msg)
{
    set_color(fd,color);
    send_message(fd,msg);
    reset_color(fd);
}


void add_new_user_to_list(int client_socket, int epoll_ds, user_context* client_list, int* current_connections_number)
{
    user_context new_user_context = {
        .offset = 0,
        .user_fd = client_socket,
    };

    memset(new_user_context.username,0,sizeof(new_user_context.username));

    client_list[(*current_connections_number)] = new_user_context;
    (*current_connections_number)++;

    printf("[%d] connected\n", client_socket);

    send_message(client_socket,"Please enter your name\n");

    struct epoll_event user_event;
    user_event.events = EPOLLIN;
    user_event.data.fd = client_socket;
    if (epoll_ctl(epoll_ds, EPOLL_CTL_ADD, client_socket, &user_event) == -1)
    {
        perror("epoll_ctl: client_socket");
        exit(EXIT_FAILURE);
    }
}

user_context* find_user_by_fd(user_context *client_list, int current_connections_number,int fd)
{
    for(int i = 0; i < current_connections_number ; i++)
    {
        if(client_list[i].user_fd == fd)
            return &client_list[i];
    }
    return NULL;
}

int count_logged_in_users(user_context* client_list, int current_connections_number)
{
    int count = 0;
    for (int i = 0; i < current_connections_number; i++)
    {
        if (client_list[i].username[0] != '\0')
            count++;
    }
    return count;
}

void send_current_users(user_context* client_list, int current_connections_number, int to_fd)
{
    send_message(to_fd, "Current users:\n");
    for (int i = 0; i < current_connections_number; i++)
    {
        if (client_list[i].username[0] != '\0')
        {
            char line[MAX_USERNAME_LENGTH + 4];
            snprintf(line, sizeof(line), "[%s]\n", client_list[i].username);
            send_message(to_fd, line);
        }
    }
}

void broadcast_user_logged_in(user_context* client_list, int current_connections_number, const char* username, int exclude_fd)
{
    char msg[128];
    snprintf(msg, sizeof(msg), "User [%s] logged in\n", username);
    for (int i = 0; i < current_connections_number; i++)
    {
        if (client_list[i].user_fd != exclude_fd && client_list[i].username[0] != '\0')
        {
            send_colored_message(client_list[i].user_fd,SOP_GREEN ,msg);
        }
    }
}

void handle_login(user_context* user, user_context* client_list, int current_connections_number)
{
    // Extract first line (username) from buffer
    char *newline = memchr(user->buf, '\n', user->offset);
    if (!newline)
        return;  // no full line yet

    size_t username_len = newline - user->buf;
    if (username_len >= MAX_USERNAME_LENGTH)
        username_len = MAX_USERNAME_LENGTH - 1;

    memcpy(user->username, user->buf, username_len);
    user->username[username_len] = '\0';

    // Shift buffer content left (remove username line)
    size_t remaining = user->offset - (username_len + 1);
    memmove(user->buf, newline + 1, remaining);
    user->offset = (int)remaining;

    printf("[%d] logged in as [%s]\n", user->user_fd, user->username);

    // Send special message if first logged-in user
    if (count_logged_in_users(client_list, current_connections_number) == 1)
    {
        send_message(user->user_fd, "You're the first one here!\n");
    }
    else
    {
        send_current_users(client_list, current_connections_number, user->user_fd);
    }

    broadcast_user_logged_in(client_list, current_connections_number, user->username, user->user_fd);
}

void broadcast_message(user_context* user_list, int current_connections_number, const char* name, int sender_fd, const char* msg)
{
    char buf[MAX_USERNAME_LENGTH + MAX_MESSAGE_SIZE + 5];
    snprintf(buf,sizeof(buf),"[%s]: %s", name,msg);

    for(int i = 0 ; i < current_connections_number; i++)
    {
        if(user_list[i].user_fd != sender_fd && user_list[i].username[0] != '\0')
        {
            send_colored_message(user_list[i].user_fd,SOP_PINK,buf);
        }
    }
}

void handle_client_message(user_context* user, user_context* client_list, int current_connections_number)
{
    // Process all complete lines in buffer
    int processed = 0;
    while (1)
    {
        char *newline = memchr(user->buf + processed, '\n', user->offset - processed);
        if (!newline)
            break;
        size_t line_len = newline - (user->buf + processed) + 1;

        // If user not logged in, treat first line as username
        if (user->username[0] == '\0')
        {
            handle_login(user, client_list, current_connections_number);
            // Note: handle_login shifts buffer and adjusts offset
            // so restart processing lines from beginning
            processed = 0;
            continue;
        }

        // Else print the message line to server console
        char line[MAX_MESSAGE_SIZE + 1];
        if (line_len > MAX_MESSAGE_SIZE)
            line_len = MAX_MESSAGE_SIZE;
        memcpy(line, user->buf + processed, line_len);
        line[line_len] = '\0';

        printf("Client [%s]: %s", user->username, line);

        broadcast_message(client_list,current_connections_number, user->username,user->user_fd, line);

        processed += line_len;
    }

    // Remove processed bytes from buffer
    if (processed > 0)
    {
        size_t remaining = user->offset - processed;
        memmove(user->buf, user->buf + processed, remaining);
        user->offset = (int)remaining;
    }
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
                int client_fd = events[i].data.fd;
                user_context *user = find_user_by_fd(client_list, current_connections_number,client_fd);
                if(user)
                {
                    ssize_t r = read(client_fd,user->buf + user->offset, MAX_MESSAGE_SIZE-user->offset);
                    if(r == 0)
                    {
                        //disconect
                        continue;
                    }
                    if(r<0)
                    {
                        if(errno == EAGAIN || errno == EWOULDBLOCK)
                            continue;
                        ERR("read");
                    }
                    user->offset = r;
                    if(user-> offset > MAX_MESSAGE_SIZE)
                        user->offset = MAX_MESSAGE_SIZE;
                    
                    handle_client_message(user, client_list,current_connections_number);
                }
                //known_user_handler(events[i]);
            }
        }
    }

    if (TEMP_FAILURE_RETRY(close(server_socket)) < 0)
        ERR("close");

    return EXIT_SUCCESS;
}
