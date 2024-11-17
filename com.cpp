#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080

void exibirStatusInicial(int num_reqs, int total_sum) {
    std::time_t t = std::time(nullptr);
    std::tm* now = std::localtime(&t);
    std::cout << "Status Inicial: " << std::endl;
    std::cout << "Data: " << (now->tm_year + 1900) << '-' 
              << (now->tm_mon + 1) << '-' 
              << now->tm_mday << std::endl;
    std::cout << "Hora: " << now->tm_hour << ':' 
              << now->tm_min << ':' 
              << now->tm_sec << std::endl;
    std::cout << "Numero de Requisicoes: " << num_reqs << std::endl;
    std::cout << "Soma Total: " << total_sum << std::endl;
}

int main() {
    int sockfd;
    char buffer[1024];
    struct sockaddr_in servaddr, cliaddr;
    int num_reqs = 0;
    int total_sum = 0;

    // Cria o socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Erro ao criar o socket" << std::endl;
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Configura o endereço do servidor
    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // Associa o socket com o endereço e porta
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "Erro ao associar o socket" << std::endl;
        close(sockfd);
        return -1;
    }

    std::cout << "Servidor UDP iniciado na porta " << PORT << std::endl;

    while (true) {
        socklen_t len = sizeof(cliaddr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&cliaddr, &len);
        buffer[n] = '\0';
        std::cout << "Cliente: " << buffer << std::endl;

        // Envia resposta ao cliente
        const char *response = "Mensagem recebida";
        sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)&cliaddr, len);
    }

    close(sockfd);
    return 0;
}