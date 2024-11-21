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
