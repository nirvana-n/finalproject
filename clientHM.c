#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUF_SIZE 2048
#define NAME_LENGHT 20
#define NAME_CLIENT 100

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[NAME_LENGHT];
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int user_id;
    char name[NAME_LENGHT];
} client_data;

void private_message(client_data *cli);
void gp_message(client_data *cli);

// output stream
void overwrite_stdout() {
    printf("\r%s", "> ");
    fflush(stdout);
}

void str_triming(char *str, int len) {
    int i;
    for (i = 0; i < len; i++) {
        if (str[i] == '\n') {
            str[i] = '\0';
            break;
        }
    }
}

void ctrl_exit() {
    flag = 1;
}

void menu(client_data *cli) {

    printf("Choose an option!\n\n 1- Group Chat\n 2- Pv Chat\n\n 3- Sending Files\n");

    int opt;
    scanf("%d", &opt);

    if (opt == 1)
        gp_message(cli);
    if (opt == 2)
        private_message(cli);
}


void gp_message(client_data *cli);


void *sending_msg_handle(void *arg) {
    client_data *cli = (client_data *) arg;
    char sent_buf[BUF_SIZE] = {};
    char msg[BUF_SIZE + NAME_LENGHT] = {};

    while (1) {
        overwrite_stdout();
        fgets(msg, BUF_SIZE, stdin);
        str_triming(msg, BUF_SIZE);

        if (strcmp(msg, "exit") == 0) {
            break;
        } else if (strcmp(msg, "back") == 0) {
            menu(cli);
        } else {
            sprintf(sent_buf, "%s-> %s\n", name, msg);
            send(sockfd, sent_buf, strlen(sent_buf), 0);
        }
        bzero(msg, BUF_SIZE + NAME_LENGHT);
        bzero(sent_buf, BUF_SIZE);
    }
    ctrl_exit(2);
}


void *receiving_msg_handle(void *arg) {
    client_data *cli = (client_data *) arg;
    char msg[BUF_SIZE] = {};
    while (1) {
        int receive = recv(sockfd, msg, BUF_SIZE, 0);
        if (receive > 0) {
            printf("%s", msg);
            overwrite_stdout();
        } else if (receive == 0) {
            break;
        } else {
            // -1
        }
        memset(msg, 0, sizeof(msg));
    }
}


void chat(client_data *cli){
  pthread_t send_msg_thread,recv_msg_thread;

  pthread_create(&send_msg_thread, NULL, &sending_msg_handle, (void *) cli);
  pthread_create(&recv_msg_thread, NULL, &receiving_msg_handle, (void *) cli);
  pthread_join((pthread_t) sending_msg_handle, NULL);
  pthread_join((pthread_t) receiving_msg_handle, NULL);

}


void pv_chat_server(client_data *cli, int port) {

    int sockfd, connfd, len;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Connection between two clients failed...\n");
        exit(0);
    }

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) {
        printf("Binding Failed...\n");
        exit(0);
    }

    // Now client_server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Ready to chat..\n");
    len = sizeof(cli);

    // Accept the data packet from client and verification
    connfd = accept(sockfd, (struct sockaddr*)&cli, &len);
    if (connfd < 0) {
        printf("Client acccept failed...\n");
        exit(0);
    }

    // Function for chatting between client and server
    chat(cli);
    // After chatting close the socket
    close(sockfd);
}


void pv_chat_client(client_data *cli, int port) {
  int sockfd, connfd;
  struct sockaddr_in servaddr;

  // socket create and varification
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
      printf("socket creation failed...\n");
      exit(0);
    }
      bzero(&servaddr, sizeof(servaddr));

// assign IP, PORT
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  servaddr.sin_port = htons(port);

// connect the client socket to server socket
  if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
      printf("connection with the server failed...\n");
      exit(0);
    }

    // function for chat
    chat(cli);

    // close the socket
    close(sockfd);
  }

void private_message(client_data *cli) {
    char name[NAME_LENGHT];
    char receiving[BUF_SIZE];
    char port[7];
    char msg[30];
    bzero(name, NAME_LENGHT);
    bzero(receiving, BUF_SIZE);
    bzero(msg, 30);
    bzero(port, 7);

    printf("Enter name: ");

    while (1) {
        scanf("%s", name);

        sprintf(msg, "name: %s\n", name);
        send(cli->sockfd, msg, strlen(msg), 0);

        printf("%s\n",msg );
        fflush(stdout);


        recv(cli->sockfd, receiving, BUF_SIZE, 0);

        printf("%s\n",receiving );
        fflush(stdout);

        if (!strcmp(receiving, "FOUND")) {
            bzero(port, 7);
            send(cli->sockfd, "myPort", strlen("myPort"), 0);
            recv(cli->sockfd, port, 7, 0);
            close(cli->sockfd);
            pv_chat_server(cli, atoi(port));
            close(cli->sockfd);
            break;
        } else if (!strcmp(receiving, "NOTFOUND")) {
            printf("not found, enter another name: ");
            continue;
        } else if (!strcmp(receiving, "CHAT")) {
            bzero(port, 7);
            bzero(msg, 30);
            strcpy(msg, "itsPort: ");
            strcat(msg, name);
            send(cli->sockfd, msg, strlen(msg), 0);
            recv(cli->sockfd, port, 7, 0);
            close(cli->sockfd);
            pv_chat_client(cli, atoi(port));
            close(cli->sockfd);
            break;
        } else {
            printf("wrong response\n");
        }
    }
}


void sending_msg_handle_msg() {
    char sent_buf[BUF_SIZE] = {};
    char msg[BUF_SIZE + NAME_LENGHT] = {};
    client_data *cli;

    while (1) {
        overwrite_stdout();
        fgets(msg, BUF_SIZE, stdin);
        str_triming(msg, BUF_SIZE);

        if (strcmp(msg, "exit") == 0) {
            break;
        } else if (strcmp(msg, "back") == 0) {
            menu(cli);
        } else {
            sprintf(sent_buf, "%s-> %s\n", name, msg);
            send(sockfd, sent_buf, strlen(sent_buf), 0);
        }
        bzero(msg, BUF_SIZE);
        bzero(sent_buf, BUF_SIZE + NAME_LENGHT);
    }
    ctrl_exit(2);
}


void receiving_msg_handle_msg() {
    char msg[BUF_SIZE] = {};
    while (1) {
        int receive = recv(sockfd, msg, BUF_SIZE, 0);
        if (receive > 0) {
            printf("%s", msg);
            overwrite_stdout();
        } else if (receive == 0) {
            break;
        } else {
            // -1
        }
        memset(msg, 0, sizeof(msg));
    }
}

void gp_message(client_data *cli) {

    pthread_t send_msg_thread;
    if (pthread_create(&send_msg_thread, NULL, (void *) sending_msg_handle_msg, NULL) != 0) {
        printf("ERROR: pthread: \n");
    }

    pthread_t recv_msg_thread;
    if (pthread_create(&recv_msg_thread, NULL, (void *) receiving_msg_handle_msg, NULL) != 0) {
        printf("ERROR: pthread: \n");
    }

    while (1) {
        if (flag == 1) {
            printf("\n BYE \n");
            break;
        }
    }
}

int main() {

    // Defining ip and sin_por
    char *ip = "127.0.0.1"; // local ip
    int port = 8080;

    char *name[NAME_LENGHT];

    signal(SIGINT, ctrl_exit);
    while (1) {
        printf("Please enter your name: ");
        fgets(name, NAME_LENGHT, stdin);
        str_triming(name, strlen(name));

        if (strlen(name) > 32 || strlen(name) < 2) {
            printf("Name must be less than 30 and more than 2 characters.\n");
            continue;
        } else
            break;
    }

    struct sockaddr_in server_addr;

    //Sockent initialize
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    //Connect client to the server
    int error = connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (error == -1) {
        printf("ERROR: Conection faild\n");
        return EXIT_FAILURE;
    }
    //Send Name
    send(sockfd, name, NAME_LENGHT, 0);

    client_data *cli = (client_data *) malloc(sizeof(client_data));
    //cli->sockfd = sockfd;
    //cli->name = name;
    //cli->address = server_addr;

    printf("=== WELCOME TO THE CHAT ===\n\n\n");

    menu(cli);

    close(sockfd);

    return EXIT_FAILURE;
}
