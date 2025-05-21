#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include "common.h"

int main(int argc, char **argv)
{
    if(argc != 2)
     {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t listen_port = atoi(argv[1]);
    if (listen_port <= 1023 || listen_port >= 65535)
    {
        fprintf(stderr, "Port must be >1023 and <65535\n");
        return EXIT_FAILURE;
    }

    int server_fd = bind_tcp_socket(listen_port, 10);

    int client_fd = add_new_client(server_fd);

    if(client_fd < 0)
        ERR("accept");

    int flags = fcntl(server_fd, F_GETFL, 0);
    if (flags < 0)
        ERR("fcntl get");
    if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) < 0)
        ERR("fcntl set");

    while(1)
    {
        int extra_fd = add_new_client(server_fd);
        if(extra_fd < 0)
            break;
        close(extra_fd);
    }
    unsigned char header[3];
    ssize_t r = bulk_read(client_fd,(char*) header, 3);
    if (r != 3)
        ERR("bulk_read header");
    printf("%s",header);
    uint8_t msg_size = header[0];
    uint16_t port_net;
    memcpy(&port_net,&header[1],2);
    uint16_t port_host = ntohs(port_net);

    char *msg = malloc(msg_size +1);
    if(!msg)
        ERR("malloc");

    r = bulk_read(client_fd, msg,msg_size);
    if(r != msg_size)
    {
        free(msg);
        ERR("bulk read");
    }
    msg[msg_size] = '\0';

    printf("Port: %u\nMessage: %s\n", port_host, msg);

    free(msg);
    close(client_fd);
    close(server_fd);

    return EXIT_SUCCESS;

}