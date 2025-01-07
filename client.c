// Parte 2 - Sistemas Operacionais 2 (2024/2) - Weverton Cordeiro
// Grupo: Bruno Alexandre - 00550177, Miguel Dutra - 00342573 e Nathan Mattes - 00342941

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
#define TIMEOUT 4

struct message {
    int type;
    int seq_num;
    int value;
    long long id_server;
};

int listen_port = 0;
int disco_port = 0;

void send_discovery_message(int sockfd, struct sockaddr_in *server_addr);
void process_server_response(int sockfd, struct sockaddr_in *server_addr);
void send_number(int sockfd, struct sockaddr_in *server_addr, int number, int seq_num);
void handle_timeout(int sockfd, struct sockaddr_in *server_addr, int number, int seq_num);
void* send_numbers(void *arg);
void exibirStatusInicial(struct sockaddr_in *server_addr);
void exibirDetalhesRequisicao(struct sockaddr_in *server_addr, int seq_num, int num_reqs, int total_sum, int req_val);
void sendKeepAliveMessage(int sockfd, struct sockaddr_in *server_addr);

int main(int argc, char *argv[]) {
    int sockfd;
    pthread_t send_thread;
    listen_port = atoi(argv[1]);
    disco_port = listen_port + 1;

    // Cria o socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("client could not create socket");
        exit(EXIT_FAILURE);
    }

    // Configura o socket para permitir broadcast
    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("client could not enable broadcast on socket");
        exit(EXIT_FAILURE);
    }

    // Envia mensagem de descoberta
    struct sockaddr_in server_addr;
    send_discovery_message(sockfd, &server_addr);

    // Processa a resposta do servidor
    process_server_response(sockfd, &server_addr);

    // Cria uma thread para enviar números automaticamente
    if (pthread_create(&send_thread, NULL, send_numbers, (void *)&sockfd) != 0) {
        perror("client error creating send thread");
        exit(EXIT_FAILURE);
    }

    // Aguarda a thread terminar (nunca termina neste caso)
    pthread_join(send_thread, NULL);

    close(sockfd);
    return 0;
}

void send_discovery_message(int sockfd, struct sockaddr_in *server_addr) {
    struct message msg;
    msg.type = 0; // Tipo de mensagem de descoberta
    msg.seq_num = 0;
    msg.value = 0;
    msg.id_server = 0;

    // Configura o endereço de broadcast
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(disco_port); // Porta de descoberta
    server_addr->sin_addr.s_addr = htonl(INADDR_BROADCAST); // Envia para todos na rede local

    // Envia a mensagem de descoberta via broadcast
    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("client erro ao enviar mensagem de descoberta");
        exit(EXIT_FAILURE);
    }
}

void process_server_response(int sockfd, struct sockaddr_in *server_addr) {
    struct message msg;
    socklen_t addr_len = sizeof(*server_addr);

    // Aguarda a resposta do servidor
    if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)server_addr, &addr_len) < 0) {
        perror("client error receiving server response");
        exit(EXIT_FAILURE);
    }

    // Exibe o status inicial
    exibirStatusInicial(server_addr);
}

void send_number(int sockfd, struct sockaddr_in *server_addr, int number, int seq_num) {
    struct message msg;
    msg.type = 1; // Request type
    msg.seq_num = seq_num;
    msg.value = number;
    msg.id_server = 0;

    // Envia o número ao servidor
    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("client error sending number");
        return;
    }
    printf("enter timeout");
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
    printf("%d\n",retval );
    if (retval == -1) {
        perror("client error in select");
    } else if (retval == 0) {
        printf("Servidor morto?");
        sendKeepAliveMessage(sockfd, server_addr);
        // Aguarda a resposta do servidor
        if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)server_addr, &addr_len) < 0) {
            perror("client error receiving server response");
            exit(EXIT_FAILURE);
        } else{
            send_number(sockfd, server_addr, number, seq_num);
        }
    } else {
        if (recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)server_addr, &addr_len) < 0) {
            perror("client error receiving ACK");
        } else {
            exibirDetalhesRequisicao(server_addr, seq_num, msg.seq_num, msg.value, number);
        }
    }
}

void* send_numbers(void *arg) {
    int sockfd = *(int *)arg;
    struct sockaddr_in server_addr;
    int number;
    int seq_num = 1; // Começa em 1, como especificado no enunciado

    // Configura o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(listen_port);
    server_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); 

     while (1) {
        if (scanf("%d", &number) != 1) {
            if (scanf("%d", &number) == EOF){
                exit(0);
            }
            fprintf(stderr, "client entrada inválida. Por favor, digite um número inteiro.\n");
            // Limpa o buffer caso a entrada não seja válida
            while (getchar() != '\n');
            continue;
        }
        // Envia o número ao servidor
        send_number(sockfd, &server_addr, number, seq_num);
        seq_num++; // Incrementa o identificador da requisição
    }
	
    return NULL;
}

// Exibe o status inicial
void exibirStatusInicial(struct sockaddr_in *server_addr) {
    time_t t = time(NULL);
    struct tm *now = localtime(&t);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(server_addr->sin_addr), ip, INET_ADDRSTRLEN);
    printf("%d-%02d-%02d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
    printf(" %02d:%02d:%02d", now->tm_hour, now->tm_min, now->tm_sec);
    printf(" server addr %s\n", ip);
}

// Exibe detalhes da requisição
void exibirDetalhesRequisicao(struct sockaddr_in *server_addr, int seq_num, int num_reqs, int total_sum, int req_val) {
    time_t t = time(NULL);
    struct tm *now = localtime(&t);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(server_addr->sin_addr), ip, INET_ADDRSTRLEN);

    printf("%d-%02d-%02d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
    printf(" %02d:%02d:%02d", now->tm_hour, now->tm_min, now->tm_sec);
    printf(" server %s", ip);
    printf(" id_req %d", seq_num);
    printf(" value %d", req_val);
    printf(" num_reqs %d", num_reqs);
    printf(" total_sum %d\n", total_sum);
}

void sendKeepAliveMessage(int sockfd, struct sockaddr_in *server_addr) {
    struct message msg;
    msg.type = 4; // Tipo de mensagem Keep Alive
    msg.seq_num = 0;
    msg.value = 0;
    msg.id_server = 0;

    // Configura o endereço de broadcast
    memset(server_addr, 0, sizeof(*server_addr));
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(disco_port); // Porta de descoberta
    server_addr->sin_addr.s_addr = htonl(INADDR_BROADCAST); // Envia para todos na rede local

    // Envia a mensagem de descoberta via broadcast
    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("client erro ao enviar mensagem de descoberta");
        exit(EXIT_FAILURE);
    }
}