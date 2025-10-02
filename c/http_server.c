#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 4096

char base_dir[1024] = ".";  // Default directory

// Get MIME type based on extension
const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "text/plain";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    return "text/plain";
}

void send_response(int client_sock, const char *status, const char *content_type, const char *body, size_t body_len) {
    char header[BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, content_type, body_len);

    write(client_sock, header, header_len);
    write(client_sock, body, body_len);
}

void serve_file(int client_sock, const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        const char *not_found = "<h1>404 Not Found</h1>";
        send_response(client_sock, "404 Not Found", "text/html", not_found, strlen(not_found));
        return;
    }

    struct stat st;
    stat(path, &st);
    size_t file_size = st.st_size;

    char *file_buf = malloc(file_size);
    if (!file_buf) {
        const char *error = "<h1>500 Internal Server Error</h1>";
        send_response(client_sock, "500 Internal Server Error", "text/html", error, strlen(error));
        fclose(file);
        return;
    }

    fread(file_buf, 1, file_size, file);
    fclose(file);

    const char *mime = get_mime_type(path);
    send_response(client_sock, "200 OK", mime, file_buf, file_size);
    free(file_buf);
}

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    int bytes_received = read(client_sock, buffer, sizeof(buffer) - 1);
    if (bytes_received <= 0) return;
    buffer[bytes_received] = '\0';

    // Basic parsing — only supports GET
    char method[8];
    char path[1024];
    sscanf(buffer, "%s %s", method, path);

    if (strcmp(method, "GET") != 0) {
        const char *error = "<h1>405 Method Not Allowed</h1>";
        send_response(client_sock, "405 Method Not Allowed", "text/html", error, strlen(error));
        return;
    }

    if (strcmp(path, "/") == 0) strcpy(path, "/index.html");

    // Build absolute path
    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s%s", base_dir, path);

    serve_file(client_sock, file_path);
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;

    // Parse args: ./http_server <port> <directory>
    if (argc >= 2)
        port = atoi(argv[1]);
    if (argc >= 3)
        strncpy(base_dir, argv[2], sizeof(base_dir) - 1);

    printf("🚀 Serving directory: %s\n", base_dir);
    printf("🌍 Listening on http://0.0.0.0:%d\n\n", port);

    int server_fd, client_sock;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        client_sock = accept(server_fd, (struct sockaddr *)&address, &addr_len);
        if (client_sock < 0) {
            perror("accept failed");
            continue;
        }

        handle_client(client_sock);
        close(client_sock);
    }

    close(server_fd);
    return 0;
}
