#include "cache.h"
#include "file.h"
#include "mime.h"
#include "net.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PORT "3490" // the port users will be connecting to

#define SERVER_FILES "./serverfiles"
#define SERVER_ROOT "./serverroot"

/**
 * Send an HTTP response
 *
 * header:       "HTTP/1.1 404 NOT FOUND" or "HTTP/1.1 200 OK", etc.
 * content_type: "text/plain", etc.
 * body:         the data to send.
 *
 * Return the value from the send() function.
 */
int send_response(int fd, char *header, char *content_type, void *body,
                  int content_length) {
    const int max_response_size = 262144;
    char response[max_response_size];

    // Build HTTP response and store it in response

    int response_length = snprintf(response, max_response_size,
                                   "%s\r\n"
                                   "Connection: close\r\n"
                                   "Content-Length: %d\r\n"
                                   "Content-Type: %s\r\n"
                                   "\r\n",
                                   header, content_length, content_type);

    // Send it all!
    int rv = send(fd, response, response_length, 0);

    if (rv < 0) {
        perror("send");
        return rv;
    }

    if (content_length > 0) {
        rv = send(fd, body, content_length, 0);
    }

    if (rv < 0) {
        perror("send");
    }

    return rv;
}

/**
 * Send a /d20 endpoint response
 */
void get_d20(int fd) {
    // Generate a random number between 1 and 20 inclusive
    int rand_num = rand() % 20 + 1;
    char resp_num[3];
    sprintf(resp_num, "%d", rand_num);

    int content_length = strlen(resp_num);

    send_response(fd, "HTTP/1.1 200 OK", "text/plain", resp_num,
                  content_length);
}

/*
 * Greet the user
 */
void greet_user(int fd, const char *name) {}

/**
 * Send a 404 response
 */
void resp_404(int fd) {
    char filepath[4096];
    struct file_data *filedata;
    char *mime_type;

    // Fetch the 404.html file
    snprintf(filepath, sizeof filepath, "%s/404.html", SERVER_FILES);
    filedata = file_load(filepath);

    if (filedata == NULL) {
        // TODO: make this non-fatal
        fprintf(stderr, "cannot find system 404 file\n");
        exit(3);
    }

    mime_type = mime_type_get(filepath);

    send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data,
                  filedata->size);

    file_free(filedata);
}

/**
 * Read and return a file from disk or cache
 */
void get_file(int fd, struct cache *cache, char *request_path) {
    struct cache_entry *existing_entry = cache_get(cache, request_path);

    if (existing_entry != NULL) {
        send_response(fd, "HTTP/1.1 200 OK", existing_entry->content_type,
                      existing_entry->content, existing_entry->content_length);
        return;
    }
    struct file_data *req_file = file_load(request_path);
    char *mime_type = mime_type_get(request_path);

    if (req_file == NULL) {
        resp_404(fd);
        return;
    }
    cache_put(cache, request_path, mime_type, req_file->data, req_file->size);

    send_response(fd, "HTTP/1.1 200 OK", mime_type, req_file->data,
                  req_file->size);
    free(req_file);

    return;
}

/**
 * Search for the end of the HTTP header
 *
 * "Newlines" in HTTP can be \r\n (carriage return followed by newline) or \n
 * (newline) or \r (carriage return).
 */
char *find_start_of_body(char *header) {
    // Look for the blank line that separates headers from body
    // Try to find \r\n\r\n first (most common)
    char *body = strstr(header, "\r\n\r\n");
    if (body != NULL) {
        return body + 4;
    }

    // Try \n\n
    body = strstr(header, "\n\n");
    if (body != NULL) {
        return body + 2;
    }

    // Try \r\r (rare, but for completeness)
    body = strstr(header, "\r\r");
    if (body != NULL) {
        return body + 2;
    }

    return NULL;
}

/**
 * Handle POST request to save data
 */
void post_save(int fd, char *body, int body_length) {
    // Generate a unique filename using timestamp
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/save_%ld.txt", SERVER_FILES,
             time(NULL));

    // Open file for writing (create if doesn't exist, truncate if exists)
    int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (file_fd < 0) {
        perror("open");
        // Send error response
        char *error_response = "{\"status\":\"error\"}";
        send_response(fd, "HTTP/1.1 500 Internal Server Error",
                      "application/json", error_response,
                      strlen(error_response));
        return;
    }

    // Write the body data to the file
    ssize_t bytes_written = write(file_fd, body, body_length);

    if (bytes_written < 0) {
        perror("write");
        close(file_fd);
        // Send error response
        char *error_response = "{\"status\":\"error\"}";
        send_response(fd, "HTTP/1.1 500 Internal Server Error",
                      "application/json", error_response,
                      strlen(error_response));
        return;
    }

    // Close the file
    close(file_fd);

    // Send success response
    char *success_response = "{\"status\":\"ok\"}";
    send_response(fd, "HTTP/1.1 200 OK", "application/json", success_response,
                  strlen(success_response));
}

/**
 * Handle HTTP request and send response
 */
void handle_http_request(int fd, struct cache *cache) {
    const int request_buffer_size = 65536; // 64K
    char request[request_buffer_size];

    // Read request
    int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);

    if (bytes_recvd < 0) {
        perror("recv");
        return;
    }

    // Read the first two components of the first line of the request
    char method[10];
    char path[100];
    char version[10];

    if (sscanf(request, "%s %s %s", method, path, version) != 3) {
        perror("Reading Header");
        return;
    }

    // If GET, handle the get endpoints
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/d20") == 0) {
            get_d20(fd);
        } else {
            char full_path[200];
            snprintf(full_path, sizeof(full_path), "./serverroot%s", path);
            get_file(fd, cache, full_path);
        }
    }
    // If POST, handle the post request
    else if (strcmp(method, "POST") == 0) {
        if (strcmp(path, "/save") == 0) {
            // Find the start of the body
            char *body = find_start_of_body(request);

            if (body != NULL) {
                // Calculate body length
                int body_length = bytes_recvd - (body - request);
                post_save(fd, body, body_length);
            } else {
                // No body found, send error
                char *error_response = "{\"status\":\"error\"}";
                send_response(fd, "HTTP/1.1 400 Bad Request",
                              "application/json", error_response,
                              strlen(error_response));
            }
        }
    }
}

/**
 * Main
 */
int main(void) {
    int newfd; // listen on sock_fd, new connection on newfd
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];
    srand(time(NULL));

    struct cache *cache = cache_create(10, 0);

    // Get a listening socket
    int listenfd = get_listener_socket(PORT);

    if (listenfd < 0) {
        fprintf(stderr, "webserver: fatal error getting listening socket\n");
        exit(1);
    }

    printf("webserver: waiting for connections on port %s...\n", PORT);

    // This is the main loop that accepts incoming connections and
    // responds to the request. The main parent process
    // then goes back to waiting for new connections.

    while (1) {
        socklen_t sin_size = sizeof their_addr;

        // Parent process will block on the accept() call until someone
        // makes a new connection:
        newfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
        if (newfd == -1) {
            perror("accept");
            continue;
        }

        // Print out a message that we got the connection
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

        // newfd is a new socket descriptor for the new connection.
        // listenfd is still listening for new connections.

        handle_http_request(newfd, cache);

        close(newfd);
    }

    // Unreachable code

    return 0;
}
