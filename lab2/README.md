# Lab 2: TCP Hello server and client

## What it teaches

- How a TCP connection is set up (3-way handshake inside `connect()` / `accept()`) and torn down (FIN from each side's `close()`).
- The structure of a socket program: `socket` -> `bind` -> `listen` -> `accept` on the server, `socket` -> `connect` on the client.
- The listening socket (`sd`) and the connected socket (`new_sd`) are different sockets.
- The client knows the server is done when `read()` returns 0, which means the server's FIN has arrived.

## Folders

| Folder | What it is |
|---|---|
| `school/` | The version demonstrated to the TA. Derived from the course's `echo_server.c` / `echo_client.c`. Builds on the Fedora lab VM. |
| `homelab/` | My own replication on my hardware and network (not started yet). |

## Build and run (school version)

```bash
cd lab2/school
make                                   # builds into build/, gcc -std=gnu89 -Wall -Wextra
./build/hello_server 5000              # terminal 1
./build/hello_client 127.0.0.1 5000    # terminal 2, prints "Hello"
make clean
```

Both programs fall back to port 3000 if no port is given.

## Homelab replication (planned)

Run the server on one host and several clients from another. Capture the 3-way handshake and the close with `tcpdump`, then open the capture in Wireshark. Show what happens to child processes with and without reaping.
