#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define SERVER_PORT 4000
#define DISCOVERY_PORT 4001
#define TIMEOUT 5

void send_discovery_message(int sockfd, struct sockaddr_in *server_addr);
void process_server_response(int sockfd, struct sockaddr_in *server_addr);
void send_number(int sockfd, struct sockaddr_in *server_addr, int number);
void handle_timeout(int sockfd, struct sockaddr_in *server_addr, int number);

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int number;

    // Cria o socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[client] Could not create socket");
        exit(EXIT_FAILURE);
    }

    // Configura o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // Envia mensagem de descoberta
    send_discovery_message(sockfd, &server_addr);

    // Processa a resposta do servidor
    process_server_response(sockfd, &server_addr);

    // Loop para enviar números ao servidor
    while (1) {
        printf("[client] Enter a number to send to the server: ");
        if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
            number = atoi(buffer);
            send_number(sockfd, &server_addr, number);
        }
    }

    close(sockfd);
    return 0;
}

void send_discovery_message(int sockfd, struct sockaddr_in *server_addr) {
    struct sockaddr_in broadcast_addr;
    char message[] = "DISCOVERY";
    int broadcast = 1;

    // Configura o endereço de broadcast
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(DISCOVERY_PORT);
    broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    // Habilita o modo de broadcast
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        perror("[client] Error setting broadcast option");
        exit(EXIT_FAILURE);
    }

    // Envia a mensagem de descoberta
    if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("[client] Error sending discovery message");
        exit(EXIT_FAILURE);
    }

    printf("[client] Discovery message sent\n");
}

void process_server_response(int sockfd, struct sockaddr_in *server_addr) {
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(*server_addr);

    // Aguarda a resposta do servidor
    if (recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)server_addr, &addr_len) < 0) {
        perror("[client] Error receiving server response");
        exit(EXIT_FAILURE);
    }

    buffer[BUFFER_SIZE - 1] = '\0';
    printf("[client] Server response: %s\n", buffer);
}

void send_number(int sockfd, struct sockaddr_in *server_addr, int number) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%d", number);

    // Envia o número ao servidor
    if (sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("[client] Error sending number");
        return;
    }

    // Configura o timeout para receber a confirmação (ACK)
    handle_timeout(sockfd, server_addr, number);
}

void handle_timeout(int sockfd, struct sockaddr_in *server_addr, int number) {
    char buffer[BUFFER_SIZE];
    struct timeval tv;
    fd_set readfds;
    socklen_t addr_len = sizeof(*server_addr);

    // Configura o timeout
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    // Aguarda a confirmação (ACK) do servidor
    int retval = select(sockfd + 1, &readfds, NULL, NULL, &tv);
    if (retval == -1) {
        perror("[client] Error in select");
    } else if (retval == 0) {
        printf("[client] Timeout, resending number: %d\n", number);
        send_number(sockfd, server_addr, number);
    } else {
        if (recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)server_addr, &addr_len) < 0) {
            perror("[client] Error receiving ACK");
        } else {
            buffer[BUFFER_SIZE - 1] = '\0';
            printf("[client] Server ACK: %s\n", buffer);
        }
    }
}