# Tutorial 8 — Sockets and epoll — Expanded Notes

---

## Headers

### `<sys/types.h>`  
- **Purpose:** Defines data types used in system calls such as `ssize_t`, `pid_t`, `off_t`.  
- **Usage:** Required by many system calls and standard libraries.

### `<sys/socket.h>`  
- **Purpose:** Socket API functions and constants.  
- **Common functions:**  
  - `socket()`, `bind()`, `listen()`, `accept()`, `connect()`, `send()`, `recv()`, `sendto()`, `recvfrom()`.  
- **Important structs:**  
  - `struct sockaddr`, `struct sockaddr_in`.

### `<netinet/in.h>`  
- **Purpose:** Defines IPv4 internet protocols and socket address structures.  
- **Important types and macros:**  
  - `struct sockaddr_in` (IPv4 socket address), `htons()`, `htonl()`, `ntohs()`, `ntohl()`.

### `<arpa/inet.h>`  
- **Purpose:** Functions for manipulating IP addresses.  
- **Common functions:**  
  - `inet_pton()`: Convert IP string to binary form.  
  - `inet_ntop()`: Convert IP binary form to string.

### `<netdb.h>`  
- **Purpose:** Network database operations.  
- **Key functions:**  
  - `getaddrinfo()`: Resolve host and service to address info.  
  - `freeaddrinfo()`: Free results from `getaddrinfo()`.  
- **Important:** Use `getaddrinfo()` instead of deprecated `gethostbyname()`.

### `<sys/un.h>`  
- **Purpose:** For Unix domain sockets, defines `struct sockaddr_un`.

### `<fcntl.h>`  
- **Purpose:** File control operations.  
- **Functions:**  
  - `fcntl()` — manipulate file descriptor flags (e.g., setting non-blocking mode).  
- **Flags:**  
  - `O_NONBLOCK`: Open/read/write operations are non-blocking.

### `<signal.h>`  
- **Purpose:** Signal handling API.  
- **Functions:**  
  - `sigaction()`, `sigprocmask()`.  
- **Common signals:**  
  - `SIGPIPE`, `SIGINT`.

### `<sys/epoll.h>`  
- **Purpose:** epoll interface for scalable I/O multiplexing on Linux.

### `<sys/time.h>`  
- **Purpose:** Timer structures and functions (`setitimer`).

### `<unistd.h>`  
- **Purpose:** POSIX standard symbolic constants and types, UNIX standard functions (`read()`, `write()`, `close()`).

### `<errno.h>`  
- **Purpose:** Defines error number macros (`errno`).

### `<stdint.h>`  
- **Purpose:** Fixed-width integer types (`int32_t`, `uint16_t`, etc.) for portability.

---

## Core Socket API Functions

### `int socket(int domain, int type, int protocol);`  
- **Description:** Creates a socket endpoint.  
- **Inputs:**  
  - `domain`: Protocol family (`PF_INET`, `PF_UNIX`, etc.).  
  - `type`: Socket type (`SOCK_STREAM`, `SOCK_DGRAM`).  
  - `protocol`: Usually 0 (default).  
- **Output:**  
  - File descriptor for the new socket, or -1 on error.

### `int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);`  
- **Description:** Assigns a local address to a socket.  
- **Inputs:**  
  - `sockfd`: Socket fd.  
  - `addr`: Pointer to socket address struct.  
  - `addrlen`: Length of address struct.  
- **Output:**  
  - 0 on success, -1 on error.

### `int listen(int sockfd, int backlog);`  
- **Description:** Marks socket as passive for incoming connections.  
- **Inputs:**  
  - `sockfd`: Socket fd.  
  - `backlog`: Max queued connections.  
- **Output:**  
  - 0 on success, -1 on error.

### `int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);`  
- **Description:** Accepts incoming connection, returns new socket fd.  
- **Inputs:**  
  - `sockfd`: Listening socket fd.  
  - `addr`: Out parameter for remote address (can be `NULL`).  
  - `addrlen`: Pointer to length of address struct.  
- **Output:**  
  - New socket fd or -1 on error.

### `int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);`  
- **Description:** Initiates a connection on a socket (client-side).  
- **Inputs:**  
  - `sockfd`: Socket fd.  
  - `addr`: Server address.  
  - `addrlen`: Length of address struct.  
- **Output:**  
  - 0 on success, -1 on error.

### `ssize_t read(int fd, void *buf, size_t count);`  
- **Description:** Read bytes from a file descriptor.  
- **Inputs:**  
  - `fd`: File descriptor.  
  - `buf`: Buffer to store data.  
  - `count`: Max bytes to read.  
- **Output:**  
  - Number of bytes read, 0 on EOF, -1 on error.

### `ssize_t write(int fd, const void *buf, size_t count);`  
- **Description:** Write bytes to a file descriptor.  
- **Inputs:**  
  - `fd`: File descriptor.  
  - `buf`: Buffer with data.  
  - `count`: Number of bytes to write.  
- **Output:**  
  - Number of bytes written, -1 on error.

---

## Address resolution

### `int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);`  
- **Description:** Resolves host and service names into socket address structures.  
- **Inputs:**  
  - `node`: Hostname or IP address string.  
  - `service`: Service name or port number string.  
  - `hints`: Optional preferences.  
  - `res`: Out parameter, points to linked list of results.  
- **Output:**  
  - 0 on success, non-zero error code on failure.  
- **Remarks:** Use `freeaddrinfo()` to free results.

### `void freeaddrinfo(struct addrinfo *res);`  
- Frees the linked list returned by `getaddrinfo()`.

---

## Byte order conversion

- `uint16_t htons(uint16_t hostshort);` — Host to network short (16-bit).  
- `uint16_t ntohs(uint16_t netshort);` — Network to host short.  
- `uint32_t htonl(uint32_t hostlong);` — Host to network long (32-bit).  
- `uint32_t ntohl(uint32_t netlong);` — Network to host long.

---

## epoll API (Linux-specific)

### `int epoll_create1(int flags);`  
- **Description:** Create an epoll instance.  
- **Input:**  
  - `flags`: 0 or `EPOLL_CLOEXEC`.  
- **Output:**  
  - epoll fd on success, -1 on error.

### `int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);`  
- **Description:** Control interface to add, modify or remove fd from epoll set.  
- **Inputs:**  
  - `epfd`: epoll fd.  
  - `op`: Operation (`EPOLL_CTL_ADD`, `EPOLL_CTL_MOD`, `EPOLL_CTL_DEL`).  
  - `fd`: Target fd.  
  - `event`: Event structure describing events and user data.  
- **Output:**  
  - 0 on success, -1 on error.

### `int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);`  
- **Description:** Wait for events on epoll fds.  
- **Inputs:**  
  - `epfd`: epoll fd.  
  - `events`: Array for returned events.  
  - `maxevents`: Max events to return.  
  - `timeout`: Timeout in ms (-1 = infinite).  
- **Output:**  
  - Number of ready fds, 0 on timeout, -1 on error.

### `int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask);`  
- Like `epoll_wait` but allows temporarily replacing signal mask during wait.

---

## `struct epoll_event`

```c
struct epoll_event {
    uint32_t events;   // Event mask (EPOLLIN, EPOLLOUT, etc.)
    union {
        void *ptr;     // User data pointer
        int fd;        // File descriptor
        uint32_t u32;
        uint64_t u64;
    } data;
};
