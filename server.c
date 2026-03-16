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
    
    // TODO: Initialize server socket, bind, listen
    // TODO: Implement main loop to accept connections and handle messages

    return 0;
}
