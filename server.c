#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "message.h"

#define MAX_CLIENTS 64
#define INACTIVITY_TIMEOUT_SEC 60

typedef struct {
    int sockfd;
    int logged_in;
    char client_id[MAX_NAME];
    char session_id[MAX_NAME];
    time_t last_active;
} client_info_t;

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

static int find_client_index_by_id(client_info_t clients[], const char *client_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd > 0 && clients[i].logged_in && strcmp(clients[i].client_id, client_id) == 0) {
            return i;
        }
    }
    return -1;
}

static int add_client(client_info_t clients[], int sockfd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd == 0) {
            clients[i].sockfd = sockfd;
            clients[i].logged_in = 0;
            memset(clients[i].client_id, 0, sizeof(clients[i].client_id));
            memset(clients[i].session_id, 0, sizeof(clients[i].session_id));
            clients[i].last_active = time(NULL);
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
    memset(clients[index].session_id, 0, sizeof(clients[index].session_id));
    clients[index].last_active = 0;
}

static void send_response(int sockfd, unsigned int type, const char *text) {
    struct message response;
    memset(&response, 0, sizeof(response));
    response.type = type;
    if (text != NULL) {
        strncpy((char *)response.data, text, MAX_DATA - 1);
        response.size = strlen((char *)response.data);
    } else {
        response.size = 0;
    }
    send_all(sockfd, &response, sizeof(response));
}

static int session_exists(client_info_t clients[], const char *session_id) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd > 0 && clients[i].logged_in && clients[i].session_id[0] != '\0' &&
            strcmp(clients[i].session_id, session_id) == 0) {
            return 1;
        }
    }
    return 0;
}

static void build_query_list(client_info_t clients[], char *out, size_t out_size) {
    out[0] = '\0';
    strncat(out, "Users online:\n", out_size - strlen(out) - 1);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd > 0 && clients[i].logged_in) {
            char line[256];
            if (clients[i].session_id[0] != '\0') {
                snprintf(line, sizeof(line), "- %s (session: %s)\n", clients[i].client_id, clients[i].session_id);
            } else {
                snprintf(line, sizeof(line), "- %s (session: none)\n", clients[i].client_id);
            }
            strncat(out, line, out_size - strlen(out) - 1);
        }
    }

    strncat(out, "Sessions:\n", out_size - strlen(out) - 1);
    char seen[MAX_CLIENTS][MAX_NAME];
    int seen_count = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd <= 0 || !clients[i].logged_in || clients[i].session_id[0] == '\0') {
            continue;
        }

        int already_seen = 0;
        for (int j = 0; j < seen_count; j++) {
            if (strcmp(seen[j], clients[i].session_id) == 0) {
                already_seen = 1;
                break;
            }
        }
        if (already_seen) {
            continue;
        }

        strncpy(seen[seen_count], clients[i].session_id, MAX_NAME - 1);
        seen[seen_count][MAX_NAME - 1] = '\0';
        seen_count++;

        int member_count = 0;
        for (int k = 0; k < MAX_CLIENTS; k++) {
            if (clients[k].sockfd > 0 && clients[k].logged_in && strcmp(clients[k].session_id, clients[i].session_id) == 0) {
                member_count++;
            }
        }

        char line[256];
        snprintf(line, sizeof(line), "- %s (%d members)\n", clients[i].session_id, member_count);
        strncat(out, line, out_size - strlen(out) - 1);
    }

    if (seen_count == 0) {
        strncat(out, "- none\n", out_size - strlen(out) - 1);
    }
}

static void multicast_to_session(client_info_t clients[], const char *session_id, const char *source, const char *text) {
    struct message forward_msg;
    memset(&forward_msg, 0, sizeof(forward_msg));
    forward_msg.type = MESSAGE;
    strncpy((char *)forward_msg.source, source, MAX_NAME - 1);
    strncpy((char *)forward_msg.data, text, MAX_DATA - 1);
    forward_msg.size = strlen((char *)forward_msg.data);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sockfd > 0 && clients[i].logged_in && strcmp(clients[i].session_id, session_id) == 0) {
            send_all(clients[i].sockfd, &forward_msg, sizeof(forward_msg));
        }
    }
}

static void send_private_message(client_info_t clients[], int sender_index, const char *target_id, const char *text) {
    int target_index = find_client_index_by_id(clients, target_id);
    if (target_index < 0) {
        send_response(clients[sender_index].sockfd, PM_NAK, "Target user is not online");
        return;
    }

    struct message direct_msg;
    memset(&direct_msg, 0, sizeof(direct_msg));
    direct_msg.type = PRIVATE_MSG;
    strncpy((char *)direct_msg.source, clients[sender_index].client_id, MAX_NAME - 1);
    strncpy((char *)direct_msg.data, text, MAX_DATA - 1);
    direct_msg.size = strlen((char *)direct_msg.data);

    send_all(clients[target_index].sockfd, &direct_msg, sizeof(direct_msg));
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

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ready = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            perror("select failed");
            continue;
        }

        if (ready > 0 && FD_ISSET(server_sockfd, &readfds)) {
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

        time_t now = time(NULL);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].sockfd > 0 && clients[i].logged_in) {
                if (difftime(now, clients[i].last_active) >= INACTIVITY_TIMEOUT_SEC) {
                    printf("Disconnecting inactive client: %s\n", clients[i].client_id);
                    send_response(clients[i].sockfd, TIMEOUT, "Disconnected due to inactivity");
                    remove_client(clients, i);
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
            if (recv_all(clients[i].sockfd, &msg, sizeof(msg)) < 0) {
                printf("Client disconnected: %s\n", clients[i].logged_in ? clients[i].client_id : "(not logged in)");
                remove_client(clients, i);
                continue;
            }

            clients[i].last_active = time(NULL);

            msg.source[MAX_NAME - 1] = '\0';
            msg.data[MAX_DATA - 1] = '\0';

            if (msg.type == LOGIN) {
                const char *client_id = (const char *)msg.source;
                const char *password = (const char *)msg.data;

                printf("Login attempt: id=%s\n", client_id);

                if (clients[i].logged_in) {
                    send_response(clients[i].sockfd, LO_NAK, "Already logged in on this connection");
                } else if (is_client_id_online(clients, client_id)) {
                    send_response(clients[i].sockfd, LO_NAK, "Client ID already online");
                } else if (!is_valid_credential(client_id, password)) {
                    send_response(clients[i].sockfd, LO_NAK, "Invalid client ID or password");
                } else {
                    clients[i].logged_in = 1;
                    strncpy(clients[i].client_id, client_id, MAX_NAME - 1);
                    clients[i].client_id[MAX_NAME - 1] = '\0';
                    clients[i].session_id[0] = '\0';
                    clients[i].last_active = time(NULL);
                    send_response(clients[i].sockfd, LO_ACK, "Welcome!");
                }
            } else if (!clients[i].logged_in) {
                send_response(clients[i].sockfd, LO_NAK, "Please login first");
            } else if (msg.type == NEW_SESS) {
                const char *new_session = (const char *)msg.data;
                if (new_session[0] == '\0') {
                    send_response(clients[i].sockfd, JN_NAK, "Session ID cannot be empty");
                } else if (clients[i].session_id[0] != '\0') {
                    send_response(clients[i].sockfd, JN_NAK, "Client already in a session");
                } else if (session_exists(clients, new_session)) {
                    send_response(clients[i].sockfd, JN_NAK, "Session already exists");
                } else {
                    strncpy(clients[i].session_id, new_session, MAX_NAME - 1);
                    clients[i].session_id[MAX_NAME - 1] = '\0';
                    send_response(clients[i].sockfd, NS_ACK, clients[i].session_id);
                }
            } else if (msg.type == JOIN) {
                const char *join_session = (const char *)msg.data;
                if (join_session[0] == '\0') {
                    send_response(clients[i].sockfd, JN_NAK, "Session ID cannot be empty");
                } else if (clients[i].session_id[0] != '\0') {
                    send_response(clients[i].sockfd, JN_NAK, "Client already in a session");
                } else if (!session_exists(clients, join_session)) {
                    send_response(clients[i].sockfd, JN_NAK, "Session does not exist");
                } else {
                    strncpy(clients[i].session_id, join_session, MAX_NAME - 1);
                    clients[i].session_id[MAX_NAME - 1] = '\0';
                    send_response(clients[i].sockfd, JN_ACK, clients[i].session_id);
                }
            } else if (msg.type == LEAVE_SESS) {
                if (clients[i].session_id[0] == '\0') {
                    send_response(clients[i].sockfd, JN_NAK, "Client is not in any session");
                } else {
                    clients[i].session_id[0] = '\0';
                    send_response(clients[i].sockfd, JN_ACK, "Left session");
                }
            } else if (msg.type == QUERY) {
                char list_buf[MAX_DATA];
                build_query_list(clients, list_buf, sizeof(list_buf));
                send_response(clients[i].sockfd, QU_ACK, list_buf);
            } else if (msg.type == MESSAGE) {
                if (clients[i].session_id[0] == '\0') {
                    send_response(clients[i].sockfd, JN_NAK, "Join a session before sending messages");
                } else {
                    multicast_to_session(clients, clients[i].session_id, clients[i].client_id, (const char *)msg.data);
                }
            } else if (msg.type == PRIVATE_MSG) {
                char target_id[MAX_NAME];
                char private_text[MAX_DATA];
                memset(target_id, 0, sizeof(target_id));
                memset(private_text, 0, sizeof(private_text));

                int parsed = sscanf((char *)msg.data, "%127s %1023[^\n]", target_id, private_text);
                if (parsed < 2 || private_text[0] == '\0') {
                    send_response(clients[i].sockfd, PM_NAK, "Usage: /pm <target-client-id> <message>");
                } else {
                    send_private_message(clients, i, target_id, private_text);
                }
            } else if (msg.type == EXIT) {
                printf("Exit request from %s\n", clients[i].logged_in ? clients[i].client_id : "(not logged in)");
                remove_client(clients, i);
            } else {
                send_response(clients[i].sockfd, LO_NAK, "Unsupported message type");
            }
        }
    }

    close(server_sockfd);
    return 0;
}
