#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/**
 * Project 1 starter code
 * All parts needed to be changed/added are marked with TODO
 */

#define BUFFER_SIZE 1024
#define DEFAULT_SERVER_PORT 8081
#define DEFAULT_REMOTE_HOST "131.179.176.34"
#define DEFAULT_REMOTE_PORT 5001

struct server_app {
    // Parameters of the server
    // Local port of HTTP server
    uint16_t server_port;

    // Remote host and port of remote proxy
    char *remote_host;
    uint16_t remote_port;
};

// The following function is implemented for you and doesn't need
// to be change
void parse_args(int argc, char *argv[], struct server_app *app);

// The following functions need to be updated
void handle_request(struct server_app *app, int client_socket);
void serve_local_file(int client_socket, const char *path, const char *file_extension);
void proxy_remote_file(struct server_app *app, int client_socket, const char *path);
char* decode_file_name(const char *input);

// The main function is provided and no change is needed
int main(int argc, char *argv[])
{
    struct server_app app;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int ret;

    parse_args(argc, argv, &app);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(app.server_port);

    // The following allows the program to immediately bind to the port in case
    // previous run exits recently
    int optval = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", app.server_port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("accept failed");
            continue;
        }
        
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        handle_request(&app, client_socket);
        close(client_socket);
    }

    close(server_socket);
    return 0;
}

void parse_args(int argc, char *argv[], struct server_app *app)
{
    int opt;

    app->server_port = DEFAULT_SERVER_PORT;
    app->remote_host = NULL;
    app->remote_port = DEFAULT_REMOTE_PORT;

    while ((opt = getopt(argc, argv, "b:r:p:")) != -1) {
        switch (opt) {
        case 'b':
            app->server_port = atoi(optarg);
            break;
        case 'r':
            app->remote_host = strdup(optarg);
            break;
        case 'p':
            app->remote_port = atoi(optarg);
            break;
        default: /* Unrecognized parameter or "-?" */
            fprintf(stderr, "Usage: server [-b local_port] [-r remote_host] [-p remote_port]\n");
            exit(-1);
            break;
        }
    }

    if (app->remote_host == NULL) {
        app->remote_host = strdup(DEFAULT_REMOTE_HOST);
    }
}

void handle_request(struct server_app *app, int client_socket) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read the request from HTTP client
    // Note: This code is not ideal in the real world because it
    // assumes that the request header is small enough and can be read
    // once as a whole.
    // However, the current version suffices for our testing.
    bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) {
        return;  // Connection closed or error
    }

    buffer[bytes_read] = '\0';
    // copy buffer to a new string
    char *request = malloc(strlen(buffer) + 1);
    strcpy(request, buffer);

    // TODO: Parse the header and extract essential fields, e.g. file name
    // Hint: if the requested path is "/" (root), default to index.html
    char *file_name = strtok(request, " ");
    file_name = strtok(NULL, " ");
    file_name = decode_file_name(file_name);
    if (file_name == NULL || strcmp(file_name, "/") == 0) {
        file_name = "index.html";
    } else {
        file_name++;
    }

    // Determine file extension
    const char *file_extension = strrchr(file_name, '.');

    // TODO: Implement proxy and call the function under condition
    // specified in the spec
    if (file_extension != NULL && isalpha(file_extension[1]) && strcmp(file_extension, ".ts") == 0) {
        proxy_remote_file(app, client_socket, buffer);
    } else {
    serve_local_file(client_socket, file_name, file_extension);
    }
}

void serve_local_file(int client_socket, const char *path, const char *file_extension) {
    // TODO: Properly implement serving of local files
    // The following code returns a dummy response for all requests
    // but it should give you a rough idea about what a proper response looks like
    // What you need to do 
    // (when the requested file exists):
    // * Open the requested file
    // * Build proper response headers (see details in the spec), and send them
    // * Also send file content
    // (When the requested file does not exist):
    // * Generate a correct response

    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        // If the file doesn't exist, generate a not found response
        char not_found_response[] = "HTTP/1.0 404 Not Found\r\n\r\n";
        send(client_socket, not_found_response, strlen(not_found_response), 0);
    } else {
        const char *content_type = "application/octet-stream"; 

        if (file_extension != NULL && isalpha(file_extension[1])) {
            // Map file extensions to content types
            if (strcmp(file_extension, ".html") == 0) {
                content_type = "text/html; charset=UTF-8";
            } else if (strcmp(file_extension, ".txt") == 0) {
                content_type = "text/plain; charset=UTF-8";
            } else if (strcmp(file_extension, ".jpg") == 0) {
                content_type = "image/jpeg";
            }
        }

        // Obtain content length
        fseek(file, 0, SEEK_END);
        long content_length = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Allocate memory for the file content
        char *file_content = malloc(content_length);

        // Obtain file content
        fread(file_content, 1, content_length, file);
        fclose(file);

        // Build response headers
        char response_headers[128];
        sprintf(response_headers,
                "HTTP/1.0 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %ld\r\n\r\n",
                content_type, content_length);

        // Send response headers
        send(client_socket, response_headers, strlen(response_headers), 0);

        // Send file content
        send(client_socket, file_content, content_length, 0);

        free(file_content);
    }
}

void proxy_remote_file(struct server_app *app, int client_socket, const char *request) {
    // TODO: Implement proxy request and replace the following code
    // What's needed:
    // * Connect to remote server (app->remote_server/app->remote_port)
    // * Forward the original request to the remote server
    // * Pass the response from remote server back
    // Bonus:
    // * When connection to the remote server fail, properly generate
    // HTTP 502 "Bad Gateway" response

    // set up new socket between server and remote
    int dest_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (dest_socket == -1) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // initialize address details
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(app->remote_port);
    dest_addr.sin_addr.s_addr = inet_addr(app->remote_host);
    memset(dest_addr.sin_zero, '\0', sizeof(dest_addr.sin_zero));

    // connect to remote server
    if (connect(dest_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1) {
        perror("socket connection failed");
        char response[] = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        exit(EXIT_FAILURE);
    }

    // forward client request
    if (send(dest_socket, request, strlen(request), 0) == -1) {
        printf("error forwarding client request");
        exit(EXIT_FAILURE);
    }

    // prepare buffer for response
    char buffer[BUFFER_SIZE];
    bzero(buffer, BUFFER_SIZE);
    int bytes_received;

    // forward incoming bytes to client
    while ((bytes_received = read(dest_socket, buffer, BUFFER_SIZE)) != 0) {
        if (send(client_socket, buffer, bytes_received, 0) == -1)
            perror("error forwarding remote response");
        bzero(buffer, BUFFER_SIZE);
    }
    
    if (bytes_received < 0) {
        perror("unable to read from remote server");
    }
}

char* decode_file_name(const char *url) {
    char *filename = malloc(strlen(url) + 1);
    int i = 0, j = 0;

    while (url[i]) {
        if (url[i] == '%' && isxdigit(url[i + 1]) && isxdigit(url[i + 2])) {
            sscanf(url + i + 1, "%2hhx", &filename[j]);
            i += 2;
        } else {
            filename[j] = url[i];
        }

        i++;
        j++;
    }

    filename[j] = '\0';
    return filename;
}