#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "message.h"

static int handle_login_command(const char *buffer, int *sockfd, char *my_client_id) {
    char client_id[MAX_NAME];
    char password[MAX_DATA];
    char server_ip[32];
    int server_port;
    struct sockaddr_in server_addr;

    if (sscanf(buffer, "/login %127s %1023s %31s %d", client_id, password, server_ip, &server_port) != 4) {
        printf("Error: Invalid login format. Usage: /login <client ID> <password> <server-IP> <server-port>\n");
        return 0;
    }

    if (*sockfd != -1) {
        printf("Already connected. Use /logout first.\n");
        return 0;
    }

    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0) {
        perror("Socket creation failed");
        *sockfd = -1;
        return 0;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid address or address not supported\n");
        close(*sockfd);
        *sockfd = -1;
        return 0;
    }

    printf("Connecting to %s:%d...\n", server_ip, server_port);
    if (connect(*sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(*sockfd);
        *sockfd = -1;
        return 0;
    }

    struct message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = LOGIN;
    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    strncpy((char *)msg.data, password, MAX_DATA - 1);
    msg.size = strlen((char *)msg.data);

    if (send(*sockfd, &msg, sizeof(msg), 0) < 0) {
        perror("Send failed");
        close(*sockfd);
        *sockfd = -1;
        return 0;
    }

    strncpy(my_client_id, client_id, MAX_NAME - 1);
    my_client_id[MAX_NAME - 1] = '\0';
    printf("Login request sent. Waiting for server response...\n");
    return 1;
}

static void send_exit_if_connected(int sockfd, const char *client_id) {
    if (sockfd == -1) {
        return;
    }

    struct message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = EXIT;
    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    msg.size = 0;
    send(sockfd, &msg, sizeof(msg), 0);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Client started. Type commands (e.g., /login <client ID> <password> <server-IP> <server-port>)\n");

    char buffer[MAX_DATA];
    int sockfd = -1;
    int logged_in = 0;
    char my_client_id[MAX_NAME] = {0};

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int max_fd = STDIN_FILENO;

        if (sockfd != -1) {
            FD_SET(sockfd, &readfds);
            if (sockfd > max_fd) {
                max_fd = sockfd;
            }
        }

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select failed");
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                send_exit_if_connected(sockfd, my_client_id);
                if (sockfd != -1) {
                    close(sockfd);
                }
                printf("Input closed. Exiting...\n");
                break;
            }

            buffer[strcspn(buffer, "\n")] = '\0';

            if (strncmp(buffer, "/login", 6) == 0) {
                if (handle_login_command(buffer, &sockfd, my_client_id)) {
                    logged_in = 0;
                }
            } else if (strcmp(buffer, "/logout") == 0) {
                if (sockfd == -1 || !logged_in) {
                    printf("You are not logged in.\n");
                } else {
                    send_exit_if_connected(sockfd, my_client_id);
                    close(sockfd);
                    sockfd = -1;
                    logged_in = 0;
                    memset(my_client_id, 0, sizeof(my_client_id));
                    printf("Logged out.\n");
                }
            } else if (strcmp(buffer, "/quit") == 0) {
                send_exit_if_connected(sockfd, my_client_id);
                if (sockfd != -1) {
                    close(sockfd);
                }
                printf("Exiting...\n");
                break;
            } else if (buffer[0] == '\0') {
                continue;
            } else {
                printf("Command not implemented yet in this phase.\n");
            }
        }

        if (sockfd != -1 && FD_ISSET(sockfd, &readfds)) {
            struct message response;
            memset(&response, 0, sizeof(response));
            int n = recv(sockfd, &response, sizeof(response), 0);
            if (n <= 0) {
                printf("Server disconnected.\n");
                close(sockfd);
                sockfd = -1;
                logged_in = 0;
                memset(my_client_id, 0, sizeof(my_client_id));
                continue;
            }

            response.source[MAX_NAME - 1] = '\0';
            response.data[MAX_DATA - 1] = '\0';

            if (response.type == LO_ACK) {
                logged_in = 1;
                printf("Login successful: %s\n", response.data);
            } else if (response.type == LO_NAK) {
                printf("Server response (NAK): %s\n", response.data);
                close(sockfd);
                sockfd = -1;
                logged_in = 0;
                memset(my_client_id, 0, sizeof(my_client_id));
            } else if (response.type == MESSAGE) {
                printf("[%s] %s\n", response.source, response.data);
            } else {
                printf("Received message type %u: %s\n", response.type, response.data);
            }
        }
    }
    
    return 0;
}
