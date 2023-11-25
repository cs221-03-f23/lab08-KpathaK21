#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 1024

volatile sig_atomic_t stop_server = 0;

void handle_signal(int signal) {
  if (signal == SIGINT) {
    printf("Server shutting down...\n");
    stop_server = 1;
  }
}

int read_port_from_file();

int create_socket(int port);
void bind_socket(int sockfd, int port);
void listen_for_connections(int sockfd);
int accept_connection(int sockfd);
void handle_client(int client_socket);

void handle_http_request(int client_socket);
void send_success_response(int client_socket);
void send_error_response(int client_socket, int status_code,
                         const char *status_text, const char *error_message);
void send_response(int client_socket, int status_code, const char *status_text,
                   const char *content_type, const char *body);

int main(int argc, char *argv[]) {

  signal(SIGINT, handle_signal);
  int port = read_port_from_file();

  int sockfd = create_socket(port);
  bind_socket(sockfd, port);
  listen_for_connections(sockfd);

  while (1) {
    int client_socket = accept_connection(sockfd);
    handle_client(client_socket);
  }

  close(sockfd);

  return 0;
}

int read_port_from_file() {
  FILE *file = fopen("port.txt", "r");
  if (file == NULL) {
    perror("Error opening port file");
    exit(EXIT_FAILURE);
  }

  int port;
  fscanf(file, "%d", &port);
  fclose(file);

  return port;
}

int create_socket(int port) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }
  return sockfd;
}

void bind_socket(int sockfd, int port) {
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("bind");
    close(sockfd);
    exit(EXIT_FAILURE);
  }
}

void listen_for_connections(int sockfd) {
  if (listen(sockfd, 3) == -1) {
    perror("listen");
    close(sockfd);
    exit(EXIT_FAILURE);
  }
}

int accept_connection(int sockfd) {
  struct sockaddr_in client_addr;
  socklen_t client_addrlen = sizeof(client_addr);
  int client_socket =
      accept(sockfd, (struct sockaddr *)&client_addr, &client_addrlen);

  if (client_socket == -1) {
    perror("accept");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  return client_socket;
}

void handle_client(int client_socket) { handle_http_request(client_socket); }

void handle_http_request(int client_socket) {
  char buffer[MAX_BUFFER_SIZE];

  // Receive the HTTP request
  ssize_t recv_val;
  while ((recv_val = recv(client_socket, buffer, MAX_BUFFER_SIZE - 1, 0)) > 0) {
    buffer[recv_val] = '\0'; // Null-terminate the received data
                             // printf("Received HTTP Request:\n%s", buffer);

    // Parse the request line
    char method[MAX_BUFFER_SIZE];
    char uri[MAX_BUFFER_SIZE];
    if (sscanf(buffer, "%s %s", method, uri) != 2) {
      fprintf(stderr, "Error parsing request line\n");
      send_error_response(client_socket, 400, "Bad Request",
                          "Invalid request format");
      return;
    }
    // Ensure the method is GET
    if (strcmp(method, "GET") != 0) {
      send_error_response(client_socket, 405, "Method Not Allowed",
                          "Only GET method is allowed");
      return;
    }

    // Ensure the URI is "/"
    if (strcmp(uri, "/") != 0) {
      send_error_response(client_socket, 404, "Not Found",
                          "Resource not found");
      return;
    }

    // Respond with a successful response
    printf("HTTP request sent, awaiting response... 200 OK\n");
    send_success_response(client_socket);
    break;
  }

  if (recv_val < 0) {
    perror("recv");
  }

  // Print a message indicating the connection is closed
  printf("Connection closed by foreign host.\n");
  close(client_socket);
}

void send_success_response(int client_socket) {
  const char *response_body =
      "<!DOCTYPE html>\n<html>\n<body>\nHello CS 221\n</body>\n</html>";
  send_response(client_socket, 200, "OK", "text/html", response_body);
}

void send_error_response(int client_socket, int status_code,
                         const char *status_text, const char *error_message) {
  const char *response_body =
      "<!DOCTYPE html>\n<html>\n<body>\nNot found\n</body>\n</html>";
  send_response(client_socket, status_code, status_text, "text/html",
                response_body);
}

void send_response(int client_socket, int status_code, const char *status_text,
                   const char *content_type, const char *body) {
  char response_header[MAX_BUFFER_SIZE];
  snprintf(response_header, MAX_BUFFER_SIZE,
           "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\n\r\n",
           status_code, status_text, content_type, strlen(body));

  ssize_t send_val =
      send(client_socket, response_header, strlen(response_header), 0);
  if (send_val < 0) {
    perror("send");
    close(client_socket);
    exit(EXIT_FAILURE);
  }

  send_val = send(client_socket, body, strlen(body), 0);
  if (send_val < 0) {
    perror("send");
    close(client_socket);
    exit(EXIT_FAILURE);
  }
}
