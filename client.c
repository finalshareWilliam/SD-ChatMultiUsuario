#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//definindo as cores de texto do chat(ANSI colors)
#define BHRED "\e[1;91m"
#define BHGRN "\e[1;92m"
#define BHYEL "\e[1;93m"
#define BHBLU "\e[1;94m"
#define BHMAG "\e[1;95m"
#define BHCYN "\e[1;96m"
#define BHWHT "\e[1;97m"
#define RESET "\e[0m"

//estrutura para definir qual o tipo de mensagem está sendo enviada
//será necessária para retornar as mensagens de erro ou sucesso 
typedef enum
{
    
    GET_USERS,
    SET_NAME,
    PUBLIC_MSG,
    PRIVATE_MSG,
    FULL,
    ERROR_NAME,
    SUCCESS,
		CONNECT,
    DISCONNECT,
    ERROR
} type_msg;

// estrutura para corpo da mensagem 
typedef struct
{
    type_msg type;
    char name[21];
    char data[256];

} body_msg;

//estrutura com informações da conexão
typedef struct data_connection
{
    int socket;
    struct sockaddr_in address;
    char name[20];
} data_connection;

//chamada das funções 
void trim (char *text);
void get_name (char *name);
void set_name (data_connection * connection);
void stop_client (data_connection * connection);
void connect_server (data_connection * connection, char *address, char *port);
void client_input (data_connection * connection);
void server_msg (data_connection * connection);

//função main se encarregará de receber os dados de entrada como usuario, endereço, porta de conexão
// a partir da criação de um socket e integrando as demais funções criar uma sessão para o usuario
// se estiver tudo ok vai criar um socket de conexão com servidor, envia os dados (libera o buffer) e encerra o socket
 
int main (int argc, char *argv[])
{
    data_connection connection;
    fd_set file_descriptors;

    if (argc != 3)
      {
          fprintf (stderr, "Usuário: %s <IP> <porta>\n", argv[0]);
          exit (1);
      }

    connect_server (&connection, argv[1], argv[2]);

    while (true)
      {
          FD_ZERO (&file_descriptors);
          FD_SET (STDIN_FILENO, &file_descriptors);
          FD_SET (connection.socket, &file_descriptors);
          fflush (stdin);

          if (select (connection.socket + 1, &file_descriptors, NULL, NULL, NULL) < 0)
            {
                perror ("Falha ao aceitar.");
                exit (1);
            }

          if (FD_ISSET (STDIN_FILENO, &file_descriptors))
            {
                client_input (&connection);
            }

          if (FD_ISSET (connection.socket, &file_descriptors))
            {
                server_msg (&connection);
            }
      }

    close (connection.socket);
    return 0;
}


//função para quebra de mensagem(não estourar o buffer)
//se o usuario enviar um texto muito grande será quebrado em partes
void trim (char *text)
{
    int len = strlen (text) - 1;
    if (text[len] == '\n')
      {
          text[len] = '\0';
      }
}

//função para pegar e inserir nome (nome de tamanho máx.20 caracteres)
//formata nome(não contabiliza espaço) e retorna erro se estiver fora do requistado 
void get_name (char *name)
{
    while (true)
      {
          printf ("Insira seu nome: ");
          fflush (stdout);
          memset (name, 0, 1000);
          fgets (name, 22, stdin);
          trim  (name);

          if (strlen (name) > 20)
            {

                puts ("O nome não pode ter mais que 20 caracteres.");

            }
          else
            {
                break;
            }
      }
}

//função para definir o nome e enviar ao chat e servidor 
// testa tamanho do nome (se dentro do permitido)e realiza a copia do dado digitado para conexão com servidor
void set_name (data_connection * connection)
{
    body_msg msg;
    msg.type = SET_NAME;
    strncpy (msg.name, connection->name, 20);

    if (send (connection->socket, (void *) &msg, sizeof (msg), 0) < 0)
      {
          perror ("Erro ao enviar");
          exit (1);
      }
}


//função para fechar conexão do cliente
void stop_client (data_connection * connection)
{
    close (connection->socket);
    exit (0);
}

//função para conexão com o servidor 
// serão recebidos as informações de conexão, endereço e porta do servidor
// o nome do usuario e dados da conexão serão testados, se estiver tudo ok o cliente será conectado
// se houver nome de usuario duplicado, diferença nos endereços de IP e porta será retornado erro 
void connect_server (data_connection * connection, char *address, char *port)
{

    while (true)
      {
          get_name (connection->name);

          if ((connection->socket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
            {
                perror ("Não foi possível criar o socket");
            }

          connection->address.sin_addr.s_addr = inet_addr (address);
          connection->address.sin_family = AF_INET;
          connection->address.sin_port = htons (atoi (port));

          if (connect (connection->socket, (struct sockaddr *) &connection->address, sizeof (connection->address)) < 0)
            {
                perror ("Erro na conexão");
                exit (1);
            }

          set_name (connection);

          body_msg msg;
          ssize_t recv_val = recv (connection->socket, &msg, sizeof (body_msg), 0);
          if (recv_val < 0)
            {
                perror ("recv falhou");
                exit (1);

            }
          else if (recv_val == 0)
            {
                close (connection->socket);
                printf ("O nome \"%s\" está em uso, por favor escolha outro!\n", connection->name);
                continue;
            }
          break;
      }

    puts (BHGRN "Conectado ao servidor." RESET);
    puts (BHYEL "Digite /ajuda ou /a para ver os comandos!" RESET);
}

// a função receberá os inputs do cliente e ficará reponsavel pelo tratamento dos comandos disponiveis na aplicação(menu de opções-> sair, ajuda, troca de mensagens)
// testará os inputs do cliente e retornará mensagens referentes ao comando solicitado
// pode também redirecionar ao usuario mensagens de erros (caso eles ocorram)

void client_input (data_connection * connection)
{
    char input[255];
    fgets (input, 255, stdin);
    trim (input);

    if (strcmp (input, "/s") == 0 || strcmp (input, "/sair") == 0)
      {
          stop_client (connection);
      }
    else if (strcmp (input, "/l") == 0 || strcmp (input, "/lista") == 0)
      {
          body_msg msg;
          msg.type = GET_USERS;

          if (send (connection->socket, &msg, sizeof (body_msg), 0) < 0)
            {
                perror ("Send failed");
                exit (1);
            }
      }
    else if (strcmp (input, "/a") == 0 || strcmp (input, "/ajuda") == 0)
      {
					puts (BHRED"---------------- Menu de opções --------------------\n");
          puts (BHYEL"1.Informações de ajuda-> /ajuda ou /a");
          puts (BHYEL"2.Lista usuarios no chat-> /lista ou /l");
          puts (BHYEL"3.Mensagem privada-> @<nome> <mensagem> ");
 					puts (BHYEL"4.Desconectar do chat ou servidor-> /sair ou /s");
					puts (BHRED"___________________________________________________" RESET);
      }
    else if (strncmp (input, "@", 1) == 0)
      {
          body_msg msg;
          msg.type = PRIVATE_MSG;

          char *toUsername, *chatMsg;

          toUsername = strtok (input + 1, " ");

          if (toUsername == NULL)
            {
                puts (BHRED "O formato para envio de mensagem privada é: @<nome> <menssagem>" RESET);
                return;
            }

          if (strlen (toUsername) == 0)
            {
                puts (BHRED "Você deve inserir um nome para enviar uma mensagem privada." RESET);
                return;
            }

          if (strlen (toUsername) > 20)
            {
                puts (BHRED "O nome pode conter entre 1 e 20 caracteres." RESET);
                return;
            }

          chatMsg = strtok (NULL, "");

          if (chatMsg == NULL)
            {
                puts (BHRED "Você deve inserir uma mensagem para enviar ao usuario." RESET);
                return;
            }

          strncpy (msg.name, toUsername, 20);
          strncpy (msg.data, chatMsg, 255);

          if (send (connection->socket, &msg, sizeof (body_msg), 0) < 0)
            {
                perror ("Erro no envio.");
                exit (1);
            }

      }
    else
      {
          body_msg msg;
          msg.type = PUBLIC_MSG;
          strncpy (msg.name, connection->name, 20);

          if (strlen (input) == 0)
            {
                return;
            }

          strncpy (msg.data, input, 255);

          if (send (connection->socket, &msg, sizeof (body_msg), 0) < 0)
            {
                perror ("Erro no envio.");
                exit (1);
            }
      }

}

// a função testará e informará os status de conexão, disponibilidade do servidor, quando usuario se conecta e desconecta, quando servidor é desconectado
// a partir da função de recebimento de informação (recv()) serão testados os tipos de mensagem (conexão, mensagem publica ou particular, capacidade) 

void server_msg (data_connection * connection)
{
    body_msg msg;

    ssize_t recv_val = recv (connection->socket, &msg, sizeof (body_msg), 0);
    if (recv_val < 0)
      {
          perror ("recv falhou");
          exit (1);

      }
    else if (recv_val == 0)
      {
          close (connection->socket);
          puts (BHRED "Servidor desconectado." RESET "\n");
          exit (0);
      }

    switch (msg.type)
      {

      case CONNECT:
          printf (BHGRN "%s se conectou." RESET "\n", msg.name);
          break;

      case DISCONNECT:
          printf (BHRED "%s se desconectou." RESET "\n", msg.name);
          break;

      case GET_USERS:
          printf (BHBLU "%s" RESET "\n", msg.data);
          break;

      case PUBLIC_MSG:
          printf (BHYEL "%s" RESET ": %s\n", msg.name, msg.data);
          break;

      case PRIVATE_MSG:
          printf (BHMAG "[PRIVADO] %s:" BHCYN " %s"RESET"\n" , msg.name, msg.data);
          break;

      case FULL:
          fprintf (stderr, BHRED "A sala de chat do servidor está muito cheia para aceitar novos clientes." RESET "\n");
          exit (0);
          break;

      /*default:
          fprintf (stderr,BHRED "O tipo de mensagem recebido é desconhecido." RESET "\n");
          break;*/
      }
}


