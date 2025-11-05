# C Web Server

HTTP/1.1 web server implemented in C using POSIX socket APIs and system calls.

## Architecture

- **Socket handling**: BSD sockets with `accept()`/`recv()`/`send()` for connection management
- **Caching**: LRU cache backed by a hashtable with doubly-linked list for O(1) eviction
- **MIME detection**: File extension-based content type resolution
- **Request parsing**: HTTP method and path extraction with support for GET and POST
- **Static file serving**: Maps URLs to filesystem paths under `./serverroot`

## Supported Features

- Static file serving with MIME type detection
- In-memory LRU cache for frequently accessed files
- Custom endpoint support
- POST endpoint (`/save`) for data persistence
- 404 handling with custom error page

## Building and Running

Clone the repository and build:

```bash
make
```

Run the server (listens on port 3490):

```bash
./server
```

Access via browser at `http://localhost:3490/` or use curl:

```bash
curl -D - http://localhost:3490/
curl -D - http://localhost:3490/d20
curl -X POST -H 'Content-Type: text/plain' -d 'data' http://localhost:3490/save
```

## Components

- `server.c` - main event loop and request handler
- `net.c/h` - socket setup and network utilities
- `cache.c/h` - LRU cache implementation
- `hashtable.c/h` - hash table with configurable hash function
- `llist.c/h` - generic linked list
- `file.c/h` - file I/O utilities
- `mime.c/h` - MIME type lookup
