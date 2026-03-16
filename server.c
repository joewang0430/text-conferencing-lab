#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "message.h"

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

    // Main loop to accept connections
    while (1) {
        client_len = sizeof(client_addr);
        if ((client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len)) < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Receive Login Message (Blocking for now, just for testing Phase 1)
        struct message msg;
        int n = recv(client_sockfd, &msg, sizeof(msg), 0);
        if (n > 0) {
            printf("Received message type: %d, size: %d, source: %s\n", msg.type, msg.size, msg.source);
            if (msg.type == LOGIN) {
                printf("Login attempt with data (password): %s\n", msg.data);
                
                // TODO: Authenticate user here
                // For now, simple ACK
                struct message ack;
                ack.type = LO_ACK;
                strcpy((char *)ack.data, "Welcome!");
                ack.size = strlen((char *)ack.data);
                send(client_sockfd, &ack, sizeof(ack), 0);
                printf("Sent LO_ACK\n");
            }
        } else {
            printf("Client disconnected or error\n");
        }
        
        close(client_sockfd); // Close for now in Phase 1 simple test
    }

    close(server_sockfd);
    return 0;
}
