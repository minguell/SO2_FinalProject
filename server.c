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
#define DISCOVERY_PORT 4001
#define BUFFER_SIZE 1024
#define NUM_MAX_CLIENT 10

// Estrutura para armazenar informações de cada cliente
struct client_info {
    struct sockaddr_in client_addr;
    int client_len;
    int last_seq_num;
    int partial_sum;
    char is_active;
};

// Estrutura para mensagens
struct message {
    int type;
    int seq_num;
    int value;
};

// Variáveis globais
struct client_info client_info_array[NUM_MAX_CLIENT];
pthread_mutex_t lock;
int total_sum = 0;
int num_reqs = 0;

// Prototipação das funções
void init_client_info();
void* client_handler(void *arg);
void process_request(struct message *msg, struct sockaddr_in *client_addr, socklen_t client_len, int sockfd);
void send_ack(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, int sum);
void exibirStatusInicial(int num_reqs, int total_sum);
int find_client(struct sockaddr_in *client_addr);
void update_client_info(int client_index, int seq_num, int value);
void handle_discovery(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len);
void read_total_sum(int *num_reqs, int *total_sum);
void write_total_sum(int value);

int main(int argc, char *argv[]) {
    int server_sockfd;
    struct sockaddr_in server_addr, client_addr;
    pthread_t threads[NUM_MAX_CLIENT];
    char buffer[BUFFER_SIZE];
    socklen_t client_len = sizeof(client_addr);

    // Porta padrão ou fornecida via argumento
    int listen_port = (argc > 1) ? atoi(argv[1]) : LISTEN_PORT;

    printf("[server] Using port: %d\n", listen_port);

    // Inicializa as informações dos clientes
    init_client_info();

    // Cria o socket UDP
    if ((server_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[server] Error creating socket");
        exit(EXIT_FAILURE);
    }
    printf("[server] Socket created.\n");

    // Configura o endereço do servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Permitir conexões de qualquer endereço IP
    server_addr.sin_port = htons(listen_port);

    // Faz o bind do socket
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[server] Error binding socket");
        exit(EXIT_FAILURE);
    }
    printf("[server] Bind done.\n");

    // Exibe o status inicial
    exibirStatusInicial(num_reqs, total_sum);

    printf("[server] Listening for clients on port %d...\n", listen_port);

    while (1) {
        int n = recvfrom(server_sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) {
            perror("[server] Error receiving data");
            continue;
        }

        struct message msg;
        memcpy(&msg, buffer, sizeof(msg));

        printf("[server] Message received: Type=%d, Seq_num=%d, Value=%d\n", msg.type, msg.seq_num, msg.value);

        if (msg.type == 0) {
            handle_discovery(server_sockfd, &client_addr, client_len);
        } else if (msg.type == 1) {
            process_request(&msg, &client_addr, client_len, server_sockfd);
        } else {
            printf("[server] Unknown message type received.\n");
        }
    }
}

// Inicializa as informações dos clientes
void init_client_info() {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < NUM_MAX_CLIENT; i++) {
        client_info_array[i].is_active = 0;
        client_info_array[i].last_seq_num = -1;
        client_info_array[i].partial_sum = 0;
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
    struct message msg;
    memcpy(&msg, buffer, sizeof(msg));

    // Processa a requisição
    process_request(&msg, &client_data->client_addr, client_data->client_len, sockfd);

    close(sockfd);
    free(client_data);
    pthread_exit(NULL);
}

// Processa a requisição recebida
void process_request(struct message *msg, struct sockaddr_in *client_addr, socklen_t client_len, int sockfd) {
    int client_index = find_client(client_addr);

    if (client_index == -1) {
        printf("[server] New client connected.\n");
        pthread_mutex_lock(&lock);
        for (int i = 0; i < NUM_MAX_CLIENT; i++) {
            if (!client_info_array[i].is_active) {
                client_info_array[i].client_addr = *client_addr;
                client_info_array[i].client_len = client_len;
                client_info_array[i].is_active = 1;
                client_info_array[i].last_seq_num = msg->seq_num;
                client_info_array[i].partial_sum = msg->value;
                break;
            }
        }
        pthread_mutex_unlock(&lock);
    } else {
        if (msg->seq_num <= client_info_array[client_index].last_seq_num) {
            printf("[server] Duplicate or out-of-order message received.\n");
            send_ack(sockfd, client_addr, client_len, client_info_array[client_index].partial_sum);
            return;
        }

        update_client_info(client_index, msg->seq_num, msg->value);
    }

    write_total_sum(msg->value);

    printf("[server] Received number: %d, Total sum: %d, Number of requests: %d\n", msg->value, total_sum, num_reqs);

    // Envia a confirmação (ACK) ao cliente
    send_ack(sockfd, client_addr, client_len, total_sum);
}

// Envia a confirmação (ACK) ao cliente
void send_ack(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, int sum) {
    struct message ack_msg;
    ack_msg.type = 1; // ACK type
    ack_msg.seq_num = 0;
    ack_msg.value = sum;

    sendto(sockfd, &ack_msg, sizeof(ack_msg), 0, (struct sockaddr *)client_addr, client_len);
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

// Encontra o índice do cliente na tabela
int find_client(struct sockaddr_in *client_addr) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < NUM_MAX_CLIENT; i++) {
        if (client_info_array[i].is_active && memcmp(&client_info_array[i].client_addr, client_addr, sizeof(struct sockaddr_in)) == 0) {
            pthread_mutex_unlock(&lock);
            return i;
        }
    }
    pthread_mutex_unlock(&lock);
    return -1;
}

// Atualiza as informações do cliente
void update_client_info(int client_index, int seq_num, int value) {
    pthread_mutex_lock(&lock);
    client_info_array[client_index].last_seq_num = seq_num;
    client_info_array[client_index].partial_sum += value;
    pthread_mutex_unlock(&lock);
}

// Lida com a mensagem de descoberta
void handle_discovery(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len) {
    printf("[server] Discovery message received from %s:%d\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));

    struct message response;
    response.type = 0; // Discovery response type
    response.seq_num = 0;
    response.value = 0;

    // Envia a resposta para a porta 4000 onde o cliente está esperando
    if (sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr *)client_addr, client_len) < 0) {
        perror("[server] Error sending discovery response");
    } else {
        printf("[server] Discovery response sent to %s:%d\n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
    }
}

// Função para leitura do total_sum e num_reqs
void read_total_sum(int *num_reqs_ptr, int *total_sum_ptr) {
    pthread_mutex_lock(&lock);
    *num_reqs_ptr = num_reqs;
    *total_sum_ptr = total_sum;
    pthread_mutex_unlock(&lock);
}

// Função para escrita do total_sum e incremento do num_reqs
void write_total_sum(int value) {
    pthread_mutex_lock(&lock);
    total_sum += value;
    num_reqs++;
    pthread_mutex_unlock(&lock);
}