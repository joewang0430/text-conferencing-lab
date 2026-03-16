#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "message.h"

int main(int argc, char *argv[]) {
    printf("Client started. Type commands (e.g., /login <client ID> <password> <server-IP> <server-port>)\n");
    
    char buffer[MAX_DATA];
    int sockfd = -1;
    struct sockaddr_in server_addr;

    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        // Remove trailing newline
        buffer[strcspn(buffer, "\n")] = 0;

        if (strncmp(buffer, "/login", 6) == 0) {
            char client_id[MAX_NAME];
            char password[MAX_DATA];
            char server_ip[32];
            int server_port;

            // Parse login command
            if (sscanf(buffer, "/login %s %s %s %d", client_id, password, server_ip, &server_port) != 4) {
                printf("Error: Invalid login format. Usage: /login <client ID> <password> <server-IP> <server-port>\n");
                continue;
            }

            // Create socket
            if (sockfd != -1) {
                printf("Already connected! Logout first.\n");
                continue;
            }

            if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                perror("Socket creation failed");
                continue;
            }

            // Prepare server address
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            
            // Convert port
            server_addr.sin_port = htons(server_port);
            
            // Convert IP
            if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
                printf("Invalid address/ Address not supported \n");
                close(sockfd);
                sockfd = -1;
                continue;
            }

            // Connect
            printf("Connecting to %s:%d...\n", server_ip, server_port);
            if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("Connection failed");
                close(sockfd);
                sockfd = -1;
                continue;
            }
            printf("Connected!\n");

            // Send LOGIN message
            struct message msg;
            msg.type = LOGIN;
            strncpy((char*)msg.source, client_id, MAX_NAME);
            strncpy((char*)msg.data, password, MAX_DATA);
            msg.size = strlen(password);

            if (send(sockfd, &msg, sizeof(msg), 0) < 0) {
                perror("Send failed");
                close(sockfd);
                sockfd = -1;
                continue;
            }
            printf("Login request sent.\n");

            // Wait for response (Phase 1: Blocking wait for ACK)
            struct message response;
            int n = recv(sockfd, &response, sizeof(response), 0);
            if (n > 0) {
                if (response.type == LO_ACK) {
                    printf("Login successful: %s\n", response.data);
                } else if (response.type == LO_NAK) {
                    printf("Login failed: %s\n", response.data);
                    close(sockfd);
                    sockfd = -1;
                } else {
                    printf("Unexpected response: %d\n", response.type);
                }
            } else {
                perror("Receive failed or server closed connection");
                close(sockfd);
                sockfd = -1;
            }
        
        } else if (strcmp(buffer, "/quit") == 0) {
            if (sockfd != -1) {
                close(sockfd);
            }
            printf("Exiting...\n");
            break;
        } else {
            printf("Unknown command or not implemented yet.\n");
        }
    }
    
    return 0;
}
