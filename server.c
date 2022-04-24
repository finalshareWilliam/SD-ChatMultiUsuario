#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

//definindo as cores de texto do chat(ANSI colors)
#define BHRED "\e[1;91m"
#define BHGRN "\e[1;92m"
#define BHWHT "\e[1;97m"
#define RESET "\e[0m"

//definição do número máximo de clientes suportados no servidor 
#define NUM_USERS 15

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
void init_server (data_connection * data_server, int port);
void public_msg (data_connection clients[], int sender, char *message_text);
void private_msg (data_connection clients[], int sender, char *name, char *message_text);
void connect_msg (data_connection * clients, int sender);
void disconnect_msg_server(data_connection * clients, char *name);
void users_list (data_connection * clients, int receiver);
void full_users_msg(int socket);
void stop_server (data_connection connection[]);
void status_client_msg (data_connection clients[], int sender);
int  construct_fd_set (fd_set * set, data_connection * data_server, data_connection clients[]);
void new_connection (data_connection * data_server, data_connection clients[]);
void server_input (data_connection clients[]);

//função main ficará responsavel por receber os arquivos descritores, criar o servidor a partir da porta inserida 
// a partir dos dados recebidos pelo cliente fará retransmissão das mensagens, conexões dos clientes,encerar conexões clientes,etc
// o servidor ficará encarregado por toda a parte do fluxo de troca de mensagens  
int main (int argc, char *argv[])
{
    puts (BHWHT"Iniciando servidor."RESET);

    fd_set file_descriptors;

    data_connection data_server;
    data_connection clients[NUM_USERS];

    int i;
    for (i = 0; i < NUM_USERS; i++)
      {
          clients[i].socket = 0;
      }

    if (argc != 2)
      {
          fprintf (stderr, "Servidor: %s <porta>\n", argv[0]);
          exit (1);
      }

    init_server (&data_server, atoi (argv[1]));

    while (true)
      {
          int max_fd = construct_fd_set (&file_descriptors, &data_server, clients);

          if (select (max_fd + 1, &file_descriptors, NULL, NULL, NULL) < 0)
            {
                perror ("Falha ao aceitar");
                stop_server (clients);
            }

          if (FD_ISSET (STDIN_FILENO, &file_descriptors))
            {
                server_input (clients);
            }

          if (FD_ISSET (data_server.socket, &file_descriptors))
            {
                new_connection (&data_server, clients);
            }

          for (i = 0; i < NUM_USERS; i++)
            {
                if (clients[i].socket > 0 && FD_ISSET (clients[i].socket, &file_descriptors))
                  {
                      status_client_msg (clients, i);
                  }
            }
      }

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

//função para iniciar a conexão com servidor
//serão recebidos as informações de conexão e a porta para criação e será atribuido um socket para porta(se não houver erros)
// se os dados estiverem ok e porta liberada, o socket será criado e o servidor estará ativo aguardando usuarios
void init_server (data_connection * data_server, int port)
{
    if ((data_server->socket = socket (AF_INET, SOCK_STREAM, 0)) < 0)
      {
          perror (BHRED "Não foi possível criar o socket" RESET "\n");
          exit (1);
      }

    data_server->address.sin_family = AF_INET;
    data_server->address.sin_addr.s_addr = INADDR_ANY;
    data_server->address.sin_port = htons (port);

    if (bind (data_server->socket, (struct sockaddr *) &data_server->address, sizeof (data_server->address)) < 0)
      {
          perror (BHRED "Erro ao atribuir porta.Porta em uso""\n");
          exit (1);
      }

    const int optVal = 1;
    const socklen_t optLen = sizeof (optVal);
    if (setsockopt (data_server->socket, SOL_SOCKET, SO_REUSEADDR, (void *) &optVal, optLen) < 0)
      {
          perror ("Erro ao definir o socket");
          exit (1);
      }

    if (listen (data_server->socket, 3) < 0)
      {
          perror ("Falha ao escutar porta.");
          exit (1);
      }

    printf (BHWHT "Aguardando conexão de usuários...\n" RESET);
}

//função para controle de envio das mensagens publicas do chat
//vai receber as informaçoes de cada cliente, o que está sendo enviado(informações) e o corpo da mensagem
// se estiver tudo ok a mensagem será enviada para o chat
//a função faz uma copia dos dados enviados pelo cliente para ser repassado ao chat 
void public_msg (data_connection clients[], int sender, char *message_text)
{
    body_msg msg;
    msg.type = PUBLIC_MSG;
    strncpy (msg.name, clients[sender].name, 20);
    strncpy (msg.data, message_text, 256);
    int i = 0;
    for (i = 0; i < NUM_USERS; i++)
      {
          if (i != sender && clients[i].socket != 0)
            {
                if (send (clients[i].socket, &msg, sizeof (msg), 0) < 0)
                  {
                      perror ("Erro ao enviar mensagem");
                      exit (1);
                  }
            }
      }
}

// a função para controle de envio das mensagens privadas
// o processo é o mesmo das mensagens publicas a diferença é que será textado se o usuario ao qual está mandando mensagem existe, se está sendo enviado algo
// em caso do usuario não exixtir (nome deve ser o mesto do chat) ou sem conteudo de mensagem(espaços podem ser enviados) será retornado erro
void private_msg (data_connection clients[], int sender, char *name, char *message_text)
{
    body_msg msg;
    msg.type = PRIVATE_MSG;
    strncpy (msg.name, clients[sender].name, 20);
    strncpy (msg.data, message_text, 256);

    int i;
    for (i = 0; i < NUM_USERS; i++)
      {
          if (i != sender && clients[i].socket != 0 && strcmp (clients[i].name, name) == 0)
            {
                if (send (clients[i].socket, &msg, sizeof (msg), 0) < 0)
                  {
                      perror ("Erro ao enviar mensagem");
                      exit (1);
                  }
                return;
            }
      }

    msg.type = ERROR_NAME;
    sprintf (msg.data, "Usuário \"%s\" não existe ou não está conectado!", name);

    if (send (clients[sender].socket, &msg, sizeof (msg), 0) < 0)
      {
          perror ("Erro ao enviar mensagem");
          exit (1);
      }

}

//função para retorno status da conexão 
// vai receber os dados do cliente e enviar um status quando cliente conectar
// são testados os tipos de mensagens(conectado), se os dados estiverem errados, retornam erro da conexão 
void connect_msg (data_connection * clients, int sender)
{
    body_msg msg;
    msg.type = CONNECT;
    strncpy (msg.name, clients[sender].name, 21);
    int i = 0;
    for (i = 0; i < NUM_USERS; i++)
      {
          if (clients[i].socket != 0)
            {
                if (i == sender)
                  {
                      msg.type = SUCCESS;
                      if (send (clients[i].socket, &msg, sizeof (msg), 0) < 0)
                        {
                            perror ("Erro no envio");
                            exit (1);
                        }
                  }
                else
                  {
                      if (send (clients[i].socket, &msg, sizeof (msg), 0) < 0)
                        {
                            perror ("Erro no envio");
                            exit (1);
                        }
                  }
            }
      }
}

//função para retorno status da conexão 
// vai receber os dados do cliente e enviar um status quando cliente desconectar
// são testados os tipos de mensagens(desconectado), se os dados estiverem errados, retornam erro da conexão 
void disconnect_msg_server(data_connection * clients, char *name)
{
    body_msg msg;
    msg.type = DISCONNECT;
    strncpy (msg.name, name, 21);
    int i = 0;
    for (i = 0; i < NUM_USERS; i++)
      {
          if (clients[i].socket != 0)
            {
                if (send (clients[i].socket, &msg, sizeof (msg), 0) < 0)
                  {
                      perror ("Erro no envio");
                      exit (1);
                  }
            }
      }
}

//função para retornar os usuarios presentes no chat
// a partir dos dados de conexão dos clientes contabiliza quantos usuarios estão presentes na sala
//retorna a informação a partir dos nomes dos usuarios
void users_list (data_connection * clients, int receiver)
{
    body_msg msg;
    msg.type = GET_USERS;
    char *list = msg.data;

    int i;
    for (i = 0; i < NUM_USERS; i++)
      {
          if (clients[i].socket != 0)
            {
                list = stpcpy (list, clients[i].name);
                list = stpcpy (list, "\n");
            }
      }

    if (send (clients[receiver].socket, &msg, sizeof (msg), 0) < 0)
      {
          perror ("Erro no envio");
          exit (1);
      }

}

// função para controle do numero de clientes
// a partir dos sockets criados para os clientes testa se o tamanho máx. foi atingido
void full_users_msg(int socket)
{
    body_msg full_message;
    full_message.type = FULL;

    if (send (socket, &full_message, sizeof (full_message), 0) < 0)
      {
          perror ("Erro no envio");
          exit (1);
      }

    close (socket);
}

//função para fechar conexão do servidor
void stop_server (data_connection connection[])
{
    int i;
    for (i = 0; i < NUM_USERS; i++)
      {
          close (connection[i].socket);
      }
    exit (0);
}

//função para retorno de mensagem de conexão e envio de mensagens publicas e privadas
// recebe os dados de informação do cliente e e envia os dados recebidos ao cliente
// quando o usuario conecta ou desconeta ao servidor uma mensagem é mostrada (no
void status_client_msg (data_connection clients[], int sender)
{
    int read_size;
    body_msg msg;

    if ((read_size = recv (clients[sender].socket, &msg, sizeof (body_msg), 0)) == 0)
      {
          printf (BHRED "%s se desconectou." RESET "\n", clients[sender].name);
          close (clients[sender].socket);
          clients[sender].socket = 0;
          disconnect_msg_server (clients, clients[sender].name);

      }
    else
      {

          switch (msg.type)
            {
            case GET_USERS:
                users_list (clients, sender);
                break;

            case SET_NAME:;
                int i;
                for (i = 0; i < NUM_USERS; i++)
                  {
                      if (clients[i].socket != 0 && strcmp (clients[i].name, msg.name) == 0)
                        {
                            close (clients[sender].socket);
                            clients[sender].socket = 0;
                            return;
                        }
                  }

                strcpy (clients[sender].name, msg.name);
                printf (BHGRN "%s se conectou." RESET "\n", clients[sender].name);
                connect_msg (clients, sender);
                break;

            case PUBLIC_MSG:
                public_msg(clients, sender, msg.data);
                break;

            case PRIVATE_MSG:
                private_msg (clients, sender, msg.name, msg.data);
                break;

            default:
                fprintf (stderr, "Mensagem inválida!.\n");
                break;
            }
      }
}

//função  com descritores para controle do fluxo de mensagem, especificando se está pronto para leitura, gravação ou tem uma condição de erro pendente
// serão recebidos informações da conexão do cliente e servidor e serão setados a inicialização dos conjunto de descritores para mensagem, o bit para o descritor
int construct_fd_set (fd_set * set, data_connection * data_server, data_connection clients[])
{
    FD_ZERO (set);
    FD_SET (STDIN_FILENO, set);
    FD_SET (data_server->socket, set);

    int max_fd = data_server->socket;
    int i;
    for (i = 0; i < NUM_USERS; i++)
      {
          if (clients[i].socket > 0)
            {
                FD_SET (clients[i].socket, set);
                if (clients[i].socket > max_fd)
                  {
                      max_fd = clients[i].socket;
                  }
            }
      }
    return max_fd;
}


//função para criar nova conexão de um cliente
//vai receber as informaçẽos da conexão e do clinete e se não houver erros nos dados e disponibilidade de usuaarios
// um socket é criado para o cliente e é retornado mensagem da criação com numero do socket  
void new_connection (data_connection * data_server, data_connection clients[])
{
    int new_socket;
    int address_len;
    new_socket = accept (data_server->socket, (struct sockaddr *) &data_server->address, (socklen_t *) & address_len);

    if (new_socket < 0)
      {
          perror ("Falha ao criar novo usuário");
          exit (1);
      }

    int i;
    for (i = 0; i < NUM_USERS; i++)
      {
          if (clients[i].socket == 0)
            {
                clients[i].socket = new_socket;
                break;

            }
          else if (i == NUM_USERS - 1)
            {
                full_users_msg(new_socket);
            }
      }
}


//função para encerar o servidor
//ao ser inserido o comando no terminal o servidor será finalizado e mensagens não serão mais enviadas
void server_input (data_connection clients[])
{
    char input[255];
    fgets (input, sizeof (input), stdin);
    trim (input);

    if (strcmp (input, "/s") == 0 || strcmp (input, "/sair") == 0)
      {
					puts (BHRED "Servidor desconectado." RESET);
					stop_server (clients);
					
					
      }
}


