#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "message.h"

static int send_all(int sockfd, const void *buf, size_t len) {
    const char *p = (const char *)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sockfd, p + sent, len - sent, 0);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static int recv_all(int sockfd, void *buf, size_t len) {
    char *p = (char *)buf;
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = recv(sockfd, p + recvd, len - recvd, 0);
        if (n <= 0) {
            return -1;
        }
        recvd += (size_t)n;
    }
    return 0;
}

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

    if (send_all(*sockfd, &msg, sizeof(msg)) < 0) {
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
    send_all(sockfd, &msg, sizeof(msg));
}

static int send_simple_request(int sockfd, unsigned int type, const char *client_id, const char *data) {
    if (sockfd == -1) {
        printf("Not connected. Please login first.\n");
        return 0;
    }

    struct message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = type;
    strncpy((char *)msg.source, client_id, MAX_NAME - 1);
    if (data != NULL) {
        strncpy((char *)msg.data, data, MAX_DATA - 1);
        msg.size = strlen((char *)msg.data);
    } else {
        msg.size = 0;
    }

    if (send_all(sockfd, &msg, sizeof(msg)) < 0) {
        perror("Send failed");
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Client started. Type commands (e.g., /login <client ID> <password> <server-IP> <server-port>)\n");

    char buffer[MAX_DATA];
    int sockfd = -1;
    int logged_in = 0;
    char my_client_id[MAX_NAME] = {0};
    char my_session_id[MAX_NAME] = {0};

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
                    memset(my_session_id, 0, sizeof(my_session_id));
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
                    memset(my_session_id, 0, sizeof(my_session_id));
                    printf("Logged out.\n");
                }
            } else if (strncmp(buffer, "/createsession", 14) == 0) {
                char session_id[MAX_NAME];
                if (sscanf(buffer, "/createsession %127s", session_id) != 1) {
                    printf("Usage: /createsession <session ID>\n");
                } else if (!logged_in) {
                    printf("Please login first.\n");
                } else {
                    send_simple_request(sockfd, NEW_SESS, my_client_id, session_id);
                }
            } else if (strncmp(buffer, "/joinsession", 12) == 0) {
                char session_id[MAX_NAME];
                if (sscanf(buffer, "/joinsession %127s", session_id) != 1) {
                    printf("Usage: /joinsession <session ID>\n");
                } else if (!logged_in) {
                    printf("Please login first.\n");
                } else {
                    send_simple_request(sockfd, JOIN, my_client_id, session_id);
                }
            } else if (strcmp(buffer, "/leavesession") == 0) {
                if (!logged_in) {
                    printf("Please login first.\n");
                } else {
                    send_simple_request(sockfd, LEAVE_SESS, my_client_id, "");
                }
            } else if (strcmp(buffer, "/list") == 0) {
                if (!logged_in) {
                    printf("Please login first.\n");
                } else {
                    send_simple_request(sockfd, QUERY, my_client_id, "");
                }
            } else if (strncmp(buffer, "/pm", 3) == 0) {
                if (!logged_in) {
                    printf("Please login first.\n");
                } else {
                    char *payload = buffer + 3;
                    while (*payload == ' ') {
                        payload++;
                    }

                    char *space = strchr(payload, ' ');
                    if (payload[0] == '\0' || space == NULL) {
                        printf("Usage: /pm <target-client-id> <message>\n");
                    } else {
                        *space = '\0';
                        char *target_id = payload;
                        char *private_text = space + 1;
                        while (*private_text == ' ') {
                            private_text++;
                        }

                        if (private_text[0] == '\0') {
                            printf("Usage: /pm <target-client-id> <message>\n");
                        } else {
                            char pm_payload[MAX_DATA];
                            snprintf(pm_payload, sizeof(pm_payload), "%s %s", target_id, private_text);
                            send_simple_request(sockfd, PRIVATE_MSG, my_client_id, pm_payload);
                        }
                    }
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
                if (!logged_in) {
                    printf("Unknown command. Please login first.\n");
                } else if (buffer[0] == '/') {
                    printf("Unknown command.\n");
                } else {
                    send_simple_request(sockfd, MESSAGE, my_client_id, buffer);
                }
            }
        }

        if (sockfd != -1 && FD_ISSET(sockfd, &readfds)) {
            struct message response;
            memset(&response, 0, sizeof(response));
            if (recv_all(sockfd, &response, sizeof(response)) < 0) {
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
                memset(my_session_id, 0, sizeof(my_session_id));
            } else if (response.type == NS_ACK) {
                strncpy(my_session_id, (char *)response.data, MAX_NAME - 1);
                my_session_id[MAX_NAME - 1] = '\0';
                printf("Session created and joined: %s\n", my_session_id);
            } else if (response.type == JN_ACK) {
                if (strcmp((char *)response.data, "Left session") == 0) {
                    my_session_id[0] = '\0';
                    printf("Left current session.\n");
                } else {
                    strncpy(my_session_id, (char *)response.data, MAX_NAME - 1);
                    my_session_id[MAX_NAME - 1] = '\0';
                    printf("Joined session: %s\n", my_session_id);
                }
            } else if (response.type == JN_NAK) {
                printf("Session operation failed: %s\n", response.data);
            } else if (response.type == QU_ACK) {
                printf("%s\n", response.data);
            } else if (response.type == MESSAGE) {
                printf("[%s] %s\n", response.source, response.data);
            } else if (response.type == PRIVATE_MSG) {
                printf("[PM from %s] %s\n", response.source, response.data);
            } else if (response.type == PM_NAK) {
                printf("Private message failed: %s\n", response.data);
            } else if (response.type == TIMEOUT) {
                printf("Server timeout: %s\n", response.data);
                close(sockfd);
                sockfd = -1;
                logged_in = 0;
                memset(my_client_id, 0, sizeof(my_client_id));
                memset(my_session_id, 0, sizeof(my_session_id));
            } else {
                printf("Received message type %u: %s\n", response.type, response.data);
            }
        }
    }
    
    return 0;
}
