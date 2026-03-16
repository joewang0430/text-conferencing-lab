#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "message.h"

#define MAX_CLIENTS 64

typedef struct {
    int sockfd;
    int logged_in;
    char client_id[MAX_NAME];
} client_info_t;

static int is_valid_credential(const char *client_id, const char *password) {
    if ((strcmp(client_id, "user1") == 0 && strcmp(password, "pass123") == 0) ||
        (strcmp(client_id, "jill") == 0 && strcmp(password, "eW94dsol") == 0) ||
        (strcmp(client_id, "jack") == 0 && strcmp(password, "432wlFd") == 0)) {
        return 1;
    }
    return 0;
}

static int is_client_id_online(client_info_t clients[], const char *client_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd > 0 && clients[i].logged_in && strcmp(clients[i].client_id, client_id) == 0) {
            return 1;
        }
    }
    return 0;
}

static int add_client(client_info_t clients[], int sockfd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd == 0) {
            clients[i].sockfd = sockfd;
            clients[i].logged_in = 0;
            memset(clients[i].client_id, 0, sizeof(clients[i].client_id));
            return i;
        }
    }
    return -1;
}

static void remove_client(client_info_t clients[], int index) {
    close(clients[index].sockfd);
    clients[index].sockfd = 0;
    clients[index].logged_in = 0;
    memset(clients[index].client_id, 0, sizeof(clients[index].client_id));
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    printf("Server started on port %s\n", argv[1]);

    int server_sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int port = atoi(argv[1]);
    client_info_t clients[MAX_CLIENTS];
    memset(clients, 0, sizeof(clients));

    // Create socket
    if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        return 1;
    }

    // Bind socket
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    // Listen
    if (listen(server_sockfd, 5) < 0) {
        perror("Listen failed");
        return 1;
    }

    printf("Listening for connections...\n");

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_sockfd, &readfds);
        int max_fd = server_sockfd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].sockfd > 0) {
                FD_SET(clients[i].sockfd, &readfds);
                if (clients[i].sockfd > max_fd) {
                    max_fd = clients[i].sockfd;
                }
            }
        }

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select failed");
            continue;
        }

        if (FD_ISSET(server_sockfd, &readfds)) {
            client_len = sizeof(client_addr);
            client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);
            if (client_sockfd < 0) {
                perror("Accept failed");
            } else {
                int index = add_client(clients, client_sockfd);
                if (index < 0) {
                    printf("Server full. Rejecting new connection.\n");
                    close(client_sockfd);
                } else {
                    printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].sockfd <= 0) {
                continue;
            }

            if (!FD_ISSET(clients[i].sockfd, &readfds)) {
                continue;
            }

            struct message msg;
            memset(&msg, 0, sizeof(msg));
            int n = recv(clients[i].sockfd, &msg, sizeof(msg), 0);
            if (n <= 0) {
                printf("Client disconnected: %s\n", clients[i].logged_in ? clients[i].client_id : "(not logged in)");
                remove_client(clients, i);
                continue;
            }

            msg.source[MAX_NAME - 1] = '\0';
            msg.data[MAX_DATA - 1] = '\0';

            if (msg.type == LOGIN) {
                struct message response;
                memset(&response, 0, sizeof(response));
                const char *client_id = (const char *)msg.source;
                const char *password = (const char *)msg.data;

                printf("Login attempt: id=%s\n", client_id);

                if (clients[i].logged_in) {
                    response.type = LO_NAK;
                    snprintf((char *)response.data, MAX_DATA, "Already logged in on this connection");
                } else if (is_client_id_online(clients, client_id)) {
                    response.type = LO_NAK;
                    snprintf((char *)response.data, MAX_DATA, "Client ID already online");
                } else if (!is_valid_credential(client_id, password)) {
                    response.type = LO_NAK;
                    snprintf((char *)response.data, MAX_DATA, "Invalid client ID or password");
                } else {
                    response.type = LO_ACK;
                    snprintf((char *)response.data, MAX_DATA, "Welcome!");
                    clients[i].logged_in = 1;
                    strncpy(clients[i].client_id, client_id, MAX_NAME - 1);
                    clients[i].client_id[MAX_NAME - 1] = '\0';
                }

                response.size = strlen((char *)response.data);
                send(clients[i].sockfd, &response, sizeof(response), 0);
            } else if (msg.type == EXIT) {
                printf("Exit request from %s\n", clients[i].logged_in ? clients[i].client_id : "(not logged in)");
                remove_client(clients, i);
            } else {
                struct message response;
                memset(&response, 0, sizeof(response));
                response.type = LO_NAK;
                snprintf((char *)response.data, MAX_DATA, "Unsupported message in current phase");
                response.size = strlen((char *)response.data);
                send(clients[i].sockfd, &response, sizeof(response), 0);
            }
        }
    }

    close(server_sockfd);
    return 0;
}
