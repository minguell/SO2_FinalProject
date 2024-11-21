# Trabalho Prático - Sistemas Operacionais II (Parte 1)

Este projeto implementa um serviço distribuído para soma de números inteiros, utilizando **threads**, **sincronização** e **comunicação via UDP**. É a primeira parte do trabalho prático da disciplina **INF01151 - Sistemas Operacionais II** no semestre 2024/2.

## Descrição

O sistema é composto por dois programas:
- **Servidor (`server.c`)**: Recebe requisições de múltiplos clientes, soma os valores enviados a uma variável acumuladora, e retorna o resultado parcial ao cliente correspondente.
- **Cliente (`client.c`)**: Envia números inteiros ao servidor e exibe as respostas.

### Funcionalidades
1. **Descoberta**:
   - O cliente utiliza mensagens de broadcast para localizar o servidor.
2. **Processamento**: 
   - O servidor processa cada requisição de forma concorrente, utilizando uma thread por requisição.
   - A soma é mantida de forma consistente com controle de exclusão mútua.
3. **Interface**:
   - Exibição de mensagens no cliente e servidor com informações sobre requisições e respostas.

### Estruturas de Dados
#### Tabelas no Servidor:
- **Tabela de Clientes**:
  - Endereço IP.
  - Última requisição processada.
  - Soma acumulada para o cliente.
- **Tabela de Soma Agregada**:
  - Total de requisições recebidas.
  - Soma acumulada global.

#### Formato de Mensagens
Estruturas de dados utilizadas para comunicação (definidas em C):
```c
struct requisicao {
    uint16_t value;     // Valor da requisição
};

struct requisicao_ack {
    uint16_t seqn;      // Número de sequência do ack
    uint16_t num_reqs;  // Quantidade de requisições processadas
    uint16_t total_sum; // Soma acumulada
};

typedef struct __packet {
    uint16_t type;      // Tipo do pacote (DESC, REQ, DESC_ACK, REQ_ACK)
    uint16_t seqn;      // Número de sequência da requisição
    union {
        struct requisicao req;
        struct requisicao_ack ack;
    };
} packet;
```
Compilação e Execução
Requisitos
Sistema Operacional: Linux
Compilador: GCC
Bibliotecas: Nenhuma adicional além das padrão para sockets.
Compilação
Para compilar os arquivos, use o comando:

bash
Copiar código
make
Isso irá gerar os executáveis servidor e cliente.

Execução
Servidor
Execute o servidor com o seguinte comando:

bash
Copiar código
./servidor <porta>
Por exemplo:

bash
Copiar código
./servidor 4000
Cliente
Execute o cliente com o seguinte comando:

bash
Copiar código
./cliente <porta>
Por exemplo:

bash
Copiar código
./cliente 4000

Depois, insira os números inteiros a serem somados, um por vez, pressionando ENTER após cada número.

### Interrupção
- O cliente pode ser encerrado com `CTRL+C` ou `CTRL+D`.

## Estrutura do Projeto

. ├── client.c # Código fonte do cliente ├── server.c # Código fonte do servidor ├── Makefile # Script de automação para compilação └── README.md # Documentação do projeto

markdown
Copiar código

## Observações
- A comunicação é baseada no protocolo UDP.
- O controle de concorrência no servidor utiliza um modelo leitor/escritor para acesso às tabelas de dados.
- A funcionalidade de retransmissão está implementada para lidar com timeouts.

## Contato
Para dúvidas ou sugestões, entre em contato pelo Moodle ou e-mail do professor responsável.
Sinta-se à vontade para ajustar conforme necessário!
