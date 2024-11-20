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

struct message {
    int type;
    int seq_num;
    int value;
};

void send_discovery_message(int sockfd, struct sockaddr_in *server_addr);
void process_server_response(int sockfd, struct sockaddr_in *server_addr);
void send_number(int sockfd, struct sockaddr_in *server_addr, int number, int seq_num);
void handle_timeout(int sockfd, struct sockaddr_in *server_addr, int number, int seq_num);

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int number;
    int seq_num = 0;

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
            send_number(sockfd, &server_addr, number, seq_num++);
        }
    }

    close(sockfd);
    return 0;
}

void send_discovery_message(int sockfd, struct sockaddr_in *server_addr) {
    struct sockaddr_in broadcast_addr;
    struct message msg;
    msg.type = 0; // Discovery type
    msg.seq_num = 0;
    msg.value = 0;
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
    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        perror("[client] Error sending discovery message");
        exit(EXIT_FAILURE);
    }

    printf("[client] Discovery message sent\n");
}

void process_server_response(int sockfd, struct sockaddr_in *server_addr) {
    struct message msg;
    socklen_t addr_len = sizeof(*server_addr);

    // Aguarda a resposta do servidor
    if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)server_addr, &addr_len) < 0) {
        perror("[client] Error receiving server response");
        exit(EXIT_FAILURE);
    }

    printf("[client] Server response received. Server address: %s:%d\n", inet_ntoa(server_addr->sin_addr), ntohs(server_addr->sin_port));
}

void send_number(int sockfd, struct sockaddr_in *server_addr, int number, int seq_num) {
    struct message msg;
    msg.type = 1; // Request type
    msg.seq_num = seq_num;
    msg.value = number;

    // Envia o número ao servidor
    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("[client] Error sending number");
        return;
    }

    printf("[client] Sent number: %d with sequence number: %d\n", number, seq_num);

    // Configura o timeout para receber a confirmação (ACK)
    handle_timeout(sockfd, server_addr, number, seq_num);
}

void handle_timeout(int sockfd, struct sockaddr_in *server_addr, int number, int seq_num) {
    struct message msg;
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
        printf("[client] Timeout, resending number: %d with sequence number: %d\n", number, seq_num);
        send_number(sockfd, server_addr, number, seq_num);
    } else {
        if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)server_addr, &addr_len) < 0) {
            perror("[client] Error receiving ACK");
        } else {
            printf("[client] Server ACK: Total sum = %d\n", msg.value);
        }
    }
}