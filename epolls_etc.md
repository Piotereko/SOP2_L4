
# Notes on epoll, fcntl, sockets, and TCP in C Networking

---

## 1. epoll (Linux I/O Multiplexing)

`epoll` is a Linux-specific API for efficiently monitoring multiple file descriptors to see if I/O is possible on any of them.

### Key Functions

- **`int epoll_create1(int flags)`**
  - Creates an epoll instance and returns its file descriptor.
  - `flags`: Usually 0. Can be `EPOLL_CLOEXEC` to set close-on-exec flag.
  - **Returns:** epoll fd, or -1 on error.

- **`int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)`**
  - Controls the interest list of an epoll instance.
  - `epfd`: epoll fd from `epoll_create1`.
  - `op`: operation, one of:
    - `EPOLL_CTL_ADD`: Add fd to the interest list.
    - `EPOLL_CTL_MOD`: Modify fd's event mask.
    - `EPOLL_CTL_DEL`: Remove fd from interest list.
  - `fd`: file descriptor to manage.
  - `event`: struct describing events to monitor.
  - **Returns:** 0 on success, -1 on error.

- **`int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)`**
  - Waits for I/O events on monitored file descriptors.
  - `events`: array to be filled with triggered events.
  - `maxevents`: max number of events to return.
  - `timeout`: wait time in ms (-1 = infinite).
  - **Returns:** number of triggered events, 0 on timeout, -1 on error.

- **`int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask)`**
  - Like `epoll_wait` but allows temporary signal mask change.

### Important Flags (in `struct epoll_event.events`)

- `EPOLLIN`: Data available to read.
- `EPOLLOUT`: Ready for writing.
- `EPOLLET`: Edge-triggered behavior.
- `EPOLLERR`: Error condition.
- `EPOLLHUP`: Hang up.

### Notes

- Epoll is scalable for large numbers of fds.
- `epoll_ctl` adds/removes/modifies fds dynamically.
- Use non-blocking sockets with epoll to avoid blocking calls.

---

## 2. fcntl (File descriptor control)

`fcntl` allows manipulation of file descriptor flags.

### Key Uses

- **Get file descriptor flags:**

  ```c
  int flags = fcntl(fd, F_GETFL);
  ```

- **Set file descriptor flags:**

  ```c
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  ```

### Important Flags

- `O_NONBLOCK`: Set non-blocking mode; reads/writes return immediately if no data.
- `F_GETFL`: Get file status flags.
- `F_SETFL`: Set file status flags.

### Notes

- Useful to set sockets as non-blocking when using epoll.
- Non-blocking sockets are essential for asynchronous I/O.

---

## 3. Sockets (General Networking API)

Sockets provide endpoints for communication over networks or locally.

### Creating a socket

```c
int sockfd = socket(int domain, int type, int protocol);
```

- `domain`:
  - `PF_INET` or `AF_INET` — IPv4 TCP/UDP.
  - `PF_UNIX` or `AF_UNIX` — Local UNIX sockets.
- `type`:
  - `SOCK_STREAM` — TCP (reliable stream).
  - `SOCK_DGRAM` — UDP (datagram).
- `protocol`: Usually 0 (default for type).

### Binding a socket

```c
bind(sockfd, struct sockaddr *addr, socklen_t addrlen);
```

- Associates socket with a local address and port.

### Listening (TCP only)

```c
listen(sockfd, backlog);
```

- Marks socket as passive, ready to accept connections.
- `backlog`: max pending connections queue length.

### Accepting connections (TCP only)

```c
int client_fd = accept(sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

- Accepts incoming connection, returns new socket fd.
- `addr` and `addrlen` can be NULL if remote address info is not needed.

### Connecting (TCP client)

```c
connect(sockfd, struct sockaddr *addr, socklen_t addrlen);
```

- Connects to remote server address.

### Sending and receiving data

- `read()`, `write()` for stream sockets.
- `sendto()`, `recvfrom()` for datagram sockets (UDP).
- `send()`, `recv()` for TCP (can specify flags).

### Closing socket

```c
close(sockfd);
```

---

## 4. TCP (Transmission Control Protocol)

- Connection-oriented reliable stream protocol.
- Guarantees in-order delivery, error detection, retransmission.
- Used via `SOCK_STREAM` sockets.

### Important Concepts

- Client/server model:
  - Server listens and accepts connections.
  - Client connects to server socket.
- After connection, both sides use the socket fd for bidirectional communication.
- Use non-blocking mode for asynchronous operations.
- Handle partial reads/writes (not guaranteed full message delivered in one call).

---

## 5. Supporting Functions and Macros (From common.h)

### TEMP\_FAILURE\_RETRY(expression)

```c
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
```

- Retries a system call if interrupted by signal (`EINTR`).
- Useful for robust I/O.

### Error Handling

```c
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
```

- Prints error message with file and line, then exits.

---

## 6. Important Data Structures

### struct sockaddr_in

```c
struct sockaddr_in {
    sa_family_t    sin_family; // AF_INET
    in_port_t      sin_port;   // port number (network byte order)
    struct in_addr sin_addr;   // IPv4 address
    unsigned char  sin_zero[8];
};
```

### struct sockaddr_un (for UNIX domain sockets)

```c
struct sockaddr_un {
    sa_family_t sun_family;    // AF_UNIX
    char        sun_path[108]; // file system path
};
```

### struct epoll_event

```c
typedef union epoll_data {
    void    *ptr;
    int      fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event {
    uint32_t events;    // Epoll events (EPOLLIN, etc.)
    epoll_data_t data;  // User data (usually fd)
};
```

---

## 7. Byte Order Conversion

Network communication must standardize integer byte order:

- `htons(uint16_t hostshort)` - host to network short (16-bit)
- `htonl(uint32_t hostlong)` - host to network long (32-bit)
- `ntohs(uint16_t netshort)` - network to host short
- `ntohl(uint32_t netlong)` - network to host long

Always convert integers before sending on the network and convert back after receiving.

---

## 8. Signals and Signal Handling

- `SIGPIPE` occurs when writing to a closed socket.
- The server blocks or ignores `SIGPIPE` to prevent termination.
- Signals can interrupt system calls causing `EINTR`.
- Use `TEMP_FAILURE_RETRY` or handle EINTR properly.

---

## Summary

- Use **epoll** with non-blocking sockets for scalable event-driven networking.
- Use **fcntl** to toggle non-blocking mode on sockets.
- Properly manage socket lifecycle: socket → bind → listen → accept (server), socket → connect (client).
- Use `read`/`write` or `recv`/`send` carefully to handle partial data.
- Always convert multibyte integers to network byte order.
- Use signals handling and macros like `TEMP_FAILURE_RETRY` for robustness.

---

# End of Notes
