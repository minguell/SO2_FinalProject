// Parte 2 - Sistemas Operacionais 2 (2024/2) - Weverton Cordeiro
// Grupo: Bruno Alexandre - 00550177, Miguel Dutra - 00342573 e Nathan Mattes - 00342941

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>

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


struct server_data {
    int im_leader;
    long long id_server; // Timestamp em microsec desde a Unix --lembrar de usar "<" para comparar
    int leader_addr;
};

// Estrutura para mensagens
struct message {
    int type;
    int seq_num;
    int value;
    long long id_server;
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
int listen_port = 0;
int disco_port = 0;
struct server_data server;

// Prototipação das funções
void init_client_info();
void* discovery_handler(void *arg);
void* listen_handler(void *arg);
void process_request(struct message *msg, struct sockaddr_in *client_addr, socklen_t client_len, int sockfd);
void send_ack(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, int sum, int num_reqs);
void exibirStatusInicial(int num_reqs, int total_sum);
int find_client(struct sockaddr_in *client_addr);
void update_client_info(int client_index, int seq_num, int value);
void handle_discovery(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len);
void handle_server_discovery(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len);
void read_total_sum(int *num_reqs, int *total_sum);
void write_total_sum(int value);
void* process_request_thread(void* arg);
void exibirDetalhesRequisicao(struct sockaddr_in *client_addr, int seq_num, int num_reqs, int total_sum, char* men, int req_val);
long long obterTimestampMicrosegundos();
void iniciarEleicao(long long id_server, int sockfd, struct sockaddr_in *server_addr, socklen_t server_len);

int main(int argc, char *argv[]) { 
    server.id_server = obterTimestampMicrosegundos();
    server.im_leader = 0;  
    server.leader_addr = 0; 

    pthread_t discovery_thread, listen_thread;
    listen_port = atoi(argv[1]);
    disco_port = listen_port + 1;
    // Inicializa as informações dos clientes
    init_client_info();

    // Exibe o status inicial
    exibirStatusInicial(num_reqs, total_sum);

    // Cria uma thread para escutar mensagens de descoberta
    if (pthread_create(&discovery_thread, NULL, discovery_handler, NULL) != 0) {
        perror("server error creating discovery thread");
        exit(EXIT_FAILURE);
    }

    // Cria uma thread para escutar requisições dos clientes
    if (pthread_create(&listen_thread, NULL, listen_handler, NULL) != 0) {
        perror("server error creating listen thread");
        exit(EXIT_FAILURE);
    }

    // Aguarda as threads terminarem (nunca terminam neste caso)
    pthread_join(discovery_thread, NULL);
    pthread_join(listen_thread, NULL);

    return 0;
}

// Thread para lidar com mensagens de descoberta
void* discovery_handler(void *arg) {
    int sockfd, sockfd2;
    struct sockaddr_in server_addr, client_addr, server2_addr, other_server_addr;
    socklen_t client_len = sizeof(client_addr), other_server_len = sizeof(other_server_addr);
    char buffer[BUFFER_SIZE];


    // Cria o socket UDP para descoberta de clients
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("server error creating client discovery socket");
        pthread_exit(NULL);
    }

    // Configura o endereço do servidor para descoberta
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Escuta de qualquer endereço IP
    server_addr.sin_port = htons(disco_port);


    // Faz o bind do socket de clientes
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("server error binding client discovery socket");
        close(sockfd);
        pthread_exit(NULL);
    }

    // Cria o socket UDP para descoberta de outros servidores
  /*  if ((sockfd2 = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("server error creating server discovery socket");
        pthread_exit(NULL);
    }

    // Configura o endereço do servidor para descoberta
    memset(&server2_addr, 0, sizeof(server2_addr));
    server2_addr.sin_family = AF_INET;
    server2_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Escuta de qualquer endereço IP
    server2_addr.sin_port = htons(disco_port);

    // Faz o bind do socket de server
    if (bind(sockfd2, (struct sockaddr *)&other_server_addr, sizeof(other_server_addr)) < 0) {
        perror("server error binding server discovery socket");
        close(sockfd2);
        pthread_exit(NULL);
    }*/

    iniciarEleicao(server.id_server, sockfd, &client_addr, client_len);

    while (1) {
        
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        
        if (n < 0) {
            perror("server error receiving discovery message");
            continue;
        }
        struct message msg;
        memcpy(&msg, buffer, sizeof(msg));

        if (msg.type == 0) { // Mensagem de descoberta
            handle_discovery(sockfd, &client_addr, client_len);
            
        } 
        
     /*   int m = recvfrom(sockfd2, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&other_server_addr, &other_server_len);
        
        if (m < 0) {
            perror("server error receiving discovery message");
            continue;
        }
        memcpy(&msg, buffer, sizeof(msg));*/
        
        if(msg.type == 2){
            if(msg.id_server > server.id_server){
                handle_server_discovery(sockfd, &client_addr, client_len);
            }
        }

        if(msg.type == 4){
            iniciarEleicao(server.id_server, sockfd, &client_addr, client_len);
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);
        
            if (n < 0) {
                server.im_leader = 1;
            } else {
                server.im_leader = 0;
            }
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

    if(server.im_leader){
        // Cria o socket UDP para comunicação padrão
        if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            perror("server error creating listen socket");
            pthread_exit(NULL);
        }

        // Configura o endereço do servidor para comunicação padrão
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Escuta de qualquer endereço IP
        server_addr.sin_port = htons(listen_port);

        // Faz o bind do socket
        if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("server error binding listen socket");
            close(sockfd);
            pthread_exit(NULL);
        }

        while (1) {
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);

            if (n < 0) {
                perror("server error receiving client message");
                continue;
            }

            struct message msg;
            memcpy(&msg, buffer, sizeof(msg));


            if (msg.type == 1) { // Mensagem de requisição
                // Aloca memória para os dados da thread
                struct request_thread_data* data = malloc(sizeof(struct request_thread_data));
                if (!data) {
                    perror("server memory allocation failed");
                    continue;
                }

                data->msg = msg;
                data->client_addr = client_addr;
                data->client_len = client_len;
                data->sockfd = sockfd;

                // Cria uma thread para processar a requisição
                pthread_t thread_id;
                if (pthread_create(&thread_id, NULL, process_request_thread, data) != 0) {
                    perror("server error creating thread for request");
                    free(data); // Libera a memória em caso de falha
                } else {
                    pthread_detach(thread_id); // Permite que a thread libere seus recursos ao finalizar
                }
            } else {
                printf("server unknown message type received.\n");

            }
        }
    }

    close(sockfd);
    pthread_exit(NULL);
}

// Processa a requisição recebida
void process_request(struct message *msg, struct sockaddr_in *client_addr, socklen_t client_len, int sockfd) {
    int client_index = find_client(client_addr);

    if (client_index == -1) {
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
            exibirDetalhesRequisicao(client_addr, msg->seq_num, num_reqs, total_sum," DUP!! ", msg->value);
            send_ack(sockfd, client_addr, client_len, client_info_array[client_index].partial_sum, num_reqs);
            return;
        }

        update_client_info(client_index, msg->seq_num, msg->value);
    }

    write_total_sum(msg->value);

    // Exibe detalhes da requisição
    exibirDetalhesRequisicao(client_addr, msg->seq_num, num_reqs, total_sum,"", msg->value);

    // Envia a confirmação (ACK) ao cliente
    send_ack(sockfd, client_addr, client_len, client_info_array[client_index].partial_sum, num_reqs);
}

// Envia a confirmação (ACK) ao cliente
void send_ack(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len, int sum, int num_reqs) {
    struct message ack_msg;
    ack_msg.type = 1; // ACK type
    ack_msg.seq_num = num_reqs;
    ack_msg.value = sum;
    ack_msg.id_server = 0;

    sendto(sockfd, &ack_msg, sizeof(ack_msg), 0, (struct sockaddr *)client_addr, client_len);
}

// Exibe o status inicial
void exibirStatusInicial(int num_reqs, int total_sum) {
    time_t t = time(NULL);
    struct tm *now = localtime(&t);
    printf("%d-%02d-%02d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
    printf(" %02d:%02d:%02d", now->tm_hour, now->tm_min, now->tm_sec);
    printf(" num_reqs %d", num_reqs);
    printf(" total_sum %d\n", total_sum);
    
}

// Função para obter o timestamp em microsegundos desde a época Unix
long long obterTimestampMicrosegundos() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    // Converte para microsegundos
    return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
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

    struct message response;
    response.type = 1; // Resposta ao cliente
    response.seq_num = 0;
    response.value = 0;
    response.id_server = 0;

    // Responde com o endereço de escuta do servidor
    if (sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr *)client_addr, client_len) < 0) {
        perror("server erro ao enviar resposta de descoberta");
    }
}

// Lida com a mensagem de descoberta
void handle_server_discovery(int sockfd, struct sockaddr_in *server_addr, socklen_t server_len) {

    struct message response;
    response.type = 3; // Resposta a eleicao
    response.seq_num = 0;
    response.value = 0;
    response.id_server = server.id_server;

    // Responde com o endereço de escuta do servidor
    if (sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr *)server_addr, server_len) < 0) {
        perror("server erro ao enviar resposta de eleicao");
    }
    printf("Servidor conectado");
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
void exibirDetalhesRequisicao(struct sockaddr_in *client_addr, int seq_num, int num_reqs, int total_sum, char* men, int req_val) {
    time_t t = time(NULL);
    struct tm *now = localtime(&t);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr->sin_addr), ip, INET_ADDRSTRLEN);

    printf("%d-%02d-%02d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday);
    printf(" %02d:%02d:%02d", now->tm_hour, now->tm_min, now->tm_sec);
    printf(" client %s", ip);
    printf("%s",men);
    printf(" id_req %d", seq_num);
    printf(" value %d", req_val);
    printf(" num_reqs %d", num_reqs);
    printf(" total_sum %d\n", total_sum);
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
            exibirDetalhesRequisicao(&(data->client_addr), data->msg.seq_num, num_reqs, total_sum," DUP!! ", data->msg.value);
            send_ack(data->sockfd, &(data->client_addr), data->client_len, total_sum, num_reqs);
            free(data); // Libera a memória alocada
            pthread_exit(NULL);
        }
        
        update_client_info(client_index, data->msg.seq_num, data->msg.value);
    }

    write_total_sum(data->msg.value);

    // Exibe detalhes da requisição
    exibirDetalhesRequisicao(&(data->client_addr), data->msg.seq_num, num_reqs, total_sum, "", data->msg.value);

    // Envia a confirmação (ACK) ao cliente
    send_ack(data->sockfd, &(data->client_addr), data->client_len, total_sum, num_reqs);

    free(data); // Libera a memória alocada
    pthread_exit(NULL);
}

void iniciarEleicao(long long id_server, int sockfd, struct sockaddr_in *server_addr, socklen_t server_len){
    struct message response;
    response.type = 2; // Mensagem de eleicao
    response.seq_num = 0;
    response.value = 0;
    response.id_server = id_server;

    // Responde com o endereço de escuta do servidor
    if (sendto(sockfd, &response, sizeof(response), 0, (struct sockaddr *)server_addr, server_len) < 0) {
        server.im_leader = 1;
    }
}
