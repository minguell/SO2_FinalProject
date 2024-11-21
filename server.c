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

struct request_thread_data {
    struct message msg;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    int sockfd;
};

// Variáveis globais
struct client_info client_info_array[NUM_MAX_CLIENT];
pthread_mutex_t lock;
int total_sum = 0;
int num_reqs = 0;

// Prototipação das funções
void init_client_info();
void* discovery_handler(void *arg);
void* listen_handler(void *arg);
void process_request(struct message *msg, struct sockaddr_in *client_addr, socklen_t client_len, int sockfd);
void send_ack(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, int sum);
void exibirStatusInicial(int num_reqs, int total_sum);
int find_client(struct sockaddr_in *client_addr);
void update_client_info(int client_index, int seq_num, int value);
void handle_discovery(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len);
void read_total_sum(int *num_reqs, int *total_sum);
void write_total_sum(int value);
void* process_request_thread(void* arg);
void exibirDetalhesRequisicao(struct sockaddr_in *client_addr, int seq_num, int num_reqs, int total_sum);

int main(int argc, char *argv[]) {
    pthread_t discovery_thread, listen_thread;

    // Inicializa as informações dos clientes
    init_client_info();

    // Exibe o status inicial
    exibirStatusInicial(num_reqs, total_sum);

    // Cria uma thread para escutar mensagens de descoberta
    if (pthread_create(&discovery_thread, NULL, discovery_handler, NULL) != 0) {
        perror("[server] Error creating discovery thread");
        exit(EXIT_FAILURE);
    }

    // Cria uma thread para escutar requisições dos clientes
    if (pthread_create(&listen_thread, NULL, listen_handler, NULL) != 0) {
        perror("[server] Error creating listen thread");
        exit(EXIT_FAILURE);
    }

    // Aguarda as threads terminarem (nunca terminam neste caso)
    pthread_join(discovery_thread, NULL);
    pthread_join(listen_thread, NULL);

    return 0;
}

// Thread para lidar com mensagens de descoberta
void* discovery_handler(void *arg) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Cria o socket UDP para descoberta
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[server] Error creating discovery socket");
        pthread_exit(NULL);
    }

    // Configura o endereço do servidor para descoberta
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Escuta de qualquer endereço IP
    server_addr.sin_port = htons(DISCOVERY_PORT);

    // Faz o bind do socket
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[server] Error binding discovery socket");
        close(sockfd);
        pthread_exit(NULL);
    }
    printf("[server] Discovery service listening on port %d...\n", DISCOVERY_PORT);

    while (1) {
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) {
            perror("[server] Error receiving discovery message");
            continue;
        }

        struct message msg;
        memcpy(&msg, buffer, sizeof(msg));

        if (msg.type == 0) { // Mensagem de descoberta
            handle_discovery(sockfd, &client_addr, client_len);
        }
    }

    close(sockfd);
    pthread_exit(NULL);
}

// Thread para lidar com requisições de clientes
void* listen_handler(void *arg) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Cria o socket UDP para comunicação padrão
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("[server] Error creating listen socket");
        pthread_exit(NULL);
    }

    // Configura o endereço do servidor para comunicação padrão
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Escuta de qualquer endereço IP
    server_addr.sin_port = htons(LISTEN_PORT);

    // Faz o bind do socket
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[server] Error binding listen socket");
        close(sockfd);
        pthread_exit(NULL);
    }
    printf("[server] Listening for client requests on port %d...\n", LISTEN_PORT);

    while (1) {
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        if (n < 0) {
            perror("[server] Error receiving client message");
            continue;
        }

        struct message msg;
        memcpy(&msg, buffer, sizeof(msg));

        if (msg.type == 1) { // Mensagem de requisição
            // Aloca memória para os dados da thread
            struct request_thread_data* data = malloc(sizeof(struct request_thread_data));
            if (!data) {
                perror("[server] Memory allocation failed");
                continue;
            }

            data->msg = msg;
            data->client_addr = client_addr;
            data->client_len = client_len;
            data->sockfd = sockfd;

            // Cria uma thread para processar a requisição
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, process_request_thread, data) != 0) {
                perror("[server] Error creating thread for request");
                free(data); // Libera a memória em caso de falha
            } else {
                pthread_detach(thread_id); // Permite que a thread libere seus recursos ao finalizar
            }
        } else {
            printf("[server] Unknown message type received.\n");
        }
    }

    close(sockfd);
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

    // Exibe detalhes da requisição
    exibirDetalhesRequisicao(client_addr, msg->seq_num, num_reqs, total_sum);

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

// Exibe detalhes da requisição
void exibirDetalhesRequisicao(struct sockaddr_in *client_addr, int seq_num, int num_reqs, int total_sum) {
    time_t t = time(NULL);
    struct tm *now = localtime(&t);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), ip, INET_ADDRSTRLEN);

    printf("Requisição Recebida:\n");
    printf("Data: %d-%02d-%02d\n", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
    printf("Hora: %02d:%02d:%02d\n", now->tm_hour, now->tm_min, now->tm_sec);
    printf("Endereço IP: %s\n", ip);
    printf("Número da Requisição: %d\n", seq_num);
    printf("Número Total de Requisições: %d\n", num_reqs);
    printf("Soma Acumulada: %d\n", total_sum);
}

void init_client_info() {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < NUM_MAX_CLIENT; i++) {
        client_info_array[i].is_active = 0;
        client_info_array[i].last_seq_num = -1;
        client_info_array[i].partial_sum = 0;
    }
    pthread_mutex_unlock(&lock);
}

void* process_request_thread(void* arg) {
    struct request_thread_data* data = (struct request_thread_data*)arg;

    int client_index = find_client(&(data->client_addr));
    if (client_index == -1) {
        printf("[server] New client connected.\n");
        pthread_mutex_lock(&lock);
        for (int i = 0; i < NUM_MAX_CLIENT; i++) {
            if (!client_info_array[i].is_active) {
                client_info_array[i].client_addr = data->client_addr;
                client_info_array[i].client_len = data->client_len;
                client_info_array[i].is_active = 1;
                client_info_array[i].last_seq_num = data->msg.seq_num;
                client_info_array[i].partial_sum = data->msg.value;
                break;
            }
        }
        pthread_mutex_unlock(&lock);
    } else {
        if (data->msg.seq_num <= client_info_array[client_index].last_seq_num) {
            printf("[server] DUP!! Cliente %s id_req %d value %d num_reqs %d total_sum %d\n",
                   inet_ntoa(data->client_addr.sin_addr),
                   data->msg.seq_num, data->msg.value,
                   num_reqs, total_sum);
            send_ack(data->sockfd, &(data->client_addr), data->client_len, total_sum);
            free(data); // Libera a memória alocada
            pthread_exit(NULL);
        }
        
        update_client_info(client_index, data->msg.seq_num, data->msg.value);
    }

    write_total_sum(data->msg.value);

    // Exibe detalhes da requisição
    exibirDetalhesRequisicao(&(data->client_addr), data->msg.seq_num, num_reqs, total_sum);

    // Envia a confirmação (ACK) ao cliente
    send_ack(data->sockfd, &(data->client_addr), data->client_len, total_sum);

    free(data); // Libera a memória alocada
    pthread_exit(NULL);
}