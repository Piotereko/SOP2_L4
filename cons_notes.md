# Notes on epoll-based Chat Server in C

## Overview
This chat server manages multiple clients using **epoll** for asynchronous I/O multiplexing. It supports user login, message broadcasting with color coding, and graceful client disconnection handling.

---

## Key Components and Concepts

### 1. User Context (`user_context`)
- Stores per-client state:
  - `username`: client's login name (max 32 chars).
  - `buf`: buffer for incoming data (max 64 bytes + null terminator).
  - `offset`: current data length in `buf`.
  - `user_fd`: client's socket file descriptor.

---

### 2. Important Functions

#### `usage(char* program_name)`
- Prints usage info and exits.
- Ensures user provides a port number within valid range (>1023, <65535).

#### `block_sigpipe()`
- Blocks `SIGPIPE` signals to avoid termination on broken pipe.
- Uses `sigprocmask` to block the signal.

#### `decline_new_user(int fd)`
- Sends "Server is full\n" message to a newly connected client when max clients reached.
- Closes the client's socket safely.

#### `send_message(int fd, const char *msg)`
- Writes a string message to the given socket.
- Checks and handles `EPIPE` errors.

#### `send_colored_message(int fd, enum SOP_COLOR color, const char *msg)`
- Sends colored text to client using ANSI escape codes.
- Calls `set_color()`, `send_message()`, then `reset_color()`.

#### `add_new_user_to_list(int client_socket, int epoll_ds, user_context* client_list, int* current_connections_number)`
- Initializes a new `user_context` for a client.
- Adds client to the epoll interest list (`EPOLLIN`).
- Sends initial prompt asking for username.

#### `find_user_by_fd(user_context *client_list, int current_connections_number, int fd)`
- Returns pointer to user context for given fd, or `NULL` if not found.

#### `count_logged_in_users(user_context* client_list, int current_connections_number)`
- Returns count of clients who have non-empty usernames (logged-in).

#### `send_current_users(user_context* client_list, int current_connections_number, int to_fd)`
- Sends a list of all logged-in usernames to specified client.

#### `broadcast_user_logged_in(user_context* client_list, int current_connections_number, const char* username, int exclude_fd)`
- Notifies all logged-in clients (except `exclude_fd`) that a new user has logged in.
- Uses green colored messages.

#### `handle_login(user_context* user, user_context* client_list, int current_connections_number)`
- Processes buffered data to extract username (first line terminated by `\n`).
- Updates client context username.
- Sends welcome message or list of current users.
- Broadcasts login event.

#### `broadcast_message(user_context* user_list, int current_connections_number, const char* name, int sender_fd, const char* msg)`
- Broadcasts chat messages from one client to all others.
- Uses pink color for messages.

#### `handle_client_message(user_context* user, user_context* client_list, int current_connections_number)`
- Processes all complete lines in the buffer.
- If client is not logged in, calls `handle_login`.
- Otherwise broadcasts message to others and prints to server console.
- Manages partial messages and buffer shifting.

---

### 3. Main Loop
- Creates TCP listening socket on specified port.
- Sets it non-blocking.
- Creates epoll instance, registers listening socket.
- Infinite loop:
  - Waits for events with `epoll_wait`.
  - Accepts new clients if ready on listen socket.
  - Adds new clients or declines if max reached.
  - For existing client sockets:
    - Reads available data.
    - Handles disconnections (print message, broadcast user gone, cleanup).
    - Updates buffer offset.
    - Calls `handle_client_message`.

---

### 4. Miscellaneous

- Uses `TEMP_FAILURE_RETRY` macro to handle EINTR on system calls.
- Properly closes sockets and removes from epoll on disconnect.
- Uses ANSI color escape sequences for enhanced terminal output.
- Client buffer management ensures no buffer overflow and handles multiple or partial lines.
- Ignores `SIGPIPE` to avoid crashes on write to closed sockets.

---

## Useful Headers and Functions

| Header         | Purpose                                        |
|----------------|------------------------------------------------|
| `<netdb.h>`    | Network database operations (getaddrinfo)     |
| `<sys/epoll.h>`| epoll API for scalable I/O multiplexing       |
| `<fcntl.h>`    | File descriptor control (fcntl)                |
| `<signal.h>`   | Signal handling                                |
| `<stdio.h>`    | Standard I/O                                   |
| `<stdlib.h>`   | Standard library functions                      |
| `<string.h>`   | String operations                              |
| `<unistd.h>`   | UNIX standard functions (read, write, close)  |
| `<errno.h>`    | Error number definitions                        |

---

### Important System Calls and Concepts

- `epoll_create1(flags)`: create epoll instance.
- `epoll_ctl(epfd, op, fd, event)`: add/remove/modifies fd in epoll interest list.
- `epoll_wait(epfd, events, maxevents, timeout)`: wait for events on fds.
- `accept(listen_fd, NULL, NULL)`: accept new TCP connection.
- `read(fd, buf, count)`: read data from fd.
- `write(fd, buf, count)`: write data to fd.
- `close(fd)`: close socket fd.
- `fcntl(fd, F_GETFL)`: get file descriptor flags.
- `fcntl(fd, F_SETFL, flags)`: set file descriptor flags (e.g., non-blocking).
- `sigprocmask()`: block/unblock signals.
- `memchr(buf, '\n', len)`: search for newline character in buffer.
- `memmove(dst, src, len)`: move memory block, safely handles overlap.
- `snprintf()`: safely format strings.
- `strlen()`: get string length.

---

### Constants and Macros

- `MAX_CLIENTS` - maximum simultaneous clients.
- `MAX_USERNAME_LENGTH` - max size of usernames.
- `MAX_MESSAGE_SIZE` - max message buffer size.
- `EPOLLIN` - epoll flag indicating data available to read.
- `O_NONBLOCK` - file flag for non-blocking I/O.
- `SIGPIPE` - signal sent on write to closed socket (ignored here).
- `TEMP_FAILURE_RETRY` - macro to retry system calls if interrupted by signals.
- `SOP_COLOR` enum for ANSI terminal color codes.

---

### Notes on Buffer Handling

- Buffer stores partial data from reads.
- Processes complete lines terminated by `\n`.
- Partially received lines remain buffered until completed.
- After processing, buffer is shifted to remove handled data.

---

### Disconnection Handling

- When `read` returns 0, client disconnected.
- Remove client from epoll, close socket.
- Broadcast disconnect message if logged in.
- Print disconnect message on server console.
- Compact client list by replacing removed client with last in array.

---

## Summary

This server demonstrates:

- Use of epoll for scalable multi-client handling.
- Non-blocking socket I/O.
- Handling partial messages and buffering.
- Clean client lifecycle management (connect, login, messaging, disconnect).
- Use of colors for client messages.
- Basic command line argument parsing and signal handling.

---

# End of Notes
