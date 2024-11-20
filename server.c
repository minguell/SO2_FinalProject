#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define LISTEN_PORT 4000
#define BUFFER_SIZE 1024
#define NUM_MAX_CLIENT 10

// Estrutura para armazenar informações de cada cliente
struct client_info {
    struct sockaddr_in client_addr;
    int client_len;
    char is_active;
};

// Variáveis globais
struct client_info client_info_array[NUM_MAX_CLIENT];
pthread_mutex_t lock;
int total_sum = 0;
int num_reqs = 0;

// Prototipação das funções
void init_client_info();
void* client_handler(void *arg);
void process_request(const char *buffer, struct sockaddr_in *client_addr, socklen_t client_len, int sockfd);
void send_ack(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, int sum);
void exibirStatusInicial(int num_reqs, int total_sum);

int main() {
    int server_sockfd;
    struct sockaddr_in server_addr, client_addr;
    pthread_t threads[NUM_MAX_CLIENT];
    char buffer[BUFFER_SIZE];
    socklen_t client_len = sizeof(client_addr);

    // Inicializa as informações dos clientes
    init_client_info();

    // Cria o socket UDP
    if ((server_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[server] Could not create socket");
        exit(EXIT_FAILURE);
    }
    printf("[server] Socket created.\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(LISTEN_PORT);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[server] Bind failed");
        exit(EXIT_FAILURE);
    }
    printf("[server] Bind done.\n");

    // Exibe o status inicial
    exibirStatusInicial(num_reqs, total_sum);

    printf("[server] Listening for clients...\n");

    while (1) {
        int n = recvfrom(server_sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) {
            perror("[server] Error receiving data");
            continue;
        }
        buffer[n] = '\0';

        // Cria uma thread para processar a requisição
        struct client_info *client_data = malloc(sizeof(struct client_info));
        client_data->client_addr = client_addr;
        client_data->client_len = client_len;
        if (pthread_create(&threads[num_reqs % NUM_MAX_CLIENT], NULL, client_handler, (void *)client_data) != 0) {
            perror("[server] Error creating thread");
            free(client_data);
            continue;
        }
    }

    close(server_sockfd);
    return 0;
}

// Inicializa as informações dos clientes
void init_client_info() {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < NUM_MAX_CLIENT; i++) {
        client_info_array[i].is_active = 0;
    }
    pthread_mutex_unlock(&lock);
}

// Função que a thread executa para cada cliente
void* client_handler(void *arg) {
    struct client_info *client_data = (struct client_info *)arg;
    char buffer[BUFFER_SIZE];
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("[server] Could not create socket");
        free(client_data);
        pthread_exit(NULL);
    }

    // Recebe a mensagem do cliente
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_data->client_addr, &client_data->client_len);
    if (n < 0) {
        perror("[server] Error receiving data");
        close(sockfd);
        free(client_data);
        pthread_exit(NULL);
    }
    buffer[n] = '\0';

    // Processa a requisição
    process_request(buffer, &client_data->client_addr, client_data->client_len, sockfd);

    close(sockfd);
    free(client_data);
    pthread_exit(NULL);
}

// Processa a requisição recebida
void process_request(const char *buffer, struct sockaddr_in *client_addr, socklen_t client_len, int sockfd) {
    int num = atoi(buffer);

    pthread_mutex_lock(&lock);
    total_sum += num;
    num_reqs++;
    pthread_mutex_unlock(&lock);

    printf("[server] Received number: %d, Total sum: %d, Number of requests: %d\n", num, total_sum, num_reqs);

    // Envia a confirmação (ACK) ao cliente
    send_ack(sockfd, client_addr, client_len, total_sum);
}

// Envia a confirmação (ACK) ao cliente
void send_ack(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, int sum) {
    char ack_message[BUFFER_SIZE];
    snprintf(ack_message, BUFFER_SIZE, "ACK: Total sum = %d", sum);
    sendto(sockfd, ack_message, strlen(ack_message), 0, (struct sockaddr *)client_addr, client_len);
}

// Exibe o status inicial
void exibirStatusInicial(int num_reqs, int total_sum) {
    time_t t = time(NULL);
    struct tm *now = localtime(&t);
    printf("Status Inicial:\n");
    printf("Data: %d-%02d-%02d\n", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
    printf("Hora: %02d:%02d:%02d\n", now->tm_hour, now->tm_min, now->tm_sec);
    printf("Número de Requisições: %d\n", num_reqs);
    printf("Soma Total: %d\n", total_sum);
}