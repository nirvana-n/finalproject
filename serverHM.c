#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CL 100
#define BUF_SIZE 2048 // 2^11
#define NAME_LENGHT 32
#define RCV_SIZE 1024
#define CHK_VAL_SIZE 10


static _Atomic unsigned int client_counter = 0; // to count number of clients
static int user_id = 10;

// Storing clients information
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int user_id;
    char name[NAME_LENGHT];
} client_t;

typedef struct {
    char person1[NAME_LENGHT];
    char person2[NAME_LENGHT];
}pv_data;

client_t *clients[MAX_CL];
pv_data *pv_list[MAX_CL];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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

// add clients to the queue
void add_to_queue(client_t *cli) {
    pthread_mutex_lock(&clients_mutex);
    int i;
    for (i = 0; i < MAX_CL; i++) {
        if (!clients[i]) {
            clients[i] = cli;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

//remove clients from queue using user id
void remove_from_queue(int user_id) {
    pthread_mutex_lock(&clients_mutex);
    int i;
    for (i = 0; i < MAX_CL; i++) {
        if (clients[i]) {
            if (clients[i]->user_id == user_id) {
                clients[i] == NULL;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// print the ip of client
void print_client_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d",
           addr.sin_addr.s_addr & 0xff,
           (addr.sin_addr.s_addr & 0xff00) >> 8,
           (addr.sin_addr.s_addr & 0xff0000) >> 16,
           (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// send message to clients, except the current user
void send_msg(char *msg, int user_id) {
    printf("in function send_msg\n" );
    pthread_mutex_lock(&clients_mutex);
    int i;
    for (i = 0; i < MAX_CL; i++) {
        if (clients[i]) {
            if (clients[i]->user_id != user_id) {
                if (send(clients[i]->sockfd, msg, strlen(msg),0) < 0) {
                    printf("Error: Can't send message to clients\n");
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


int name_found(client_t *cli, char *name) {
    int i;
    for (int i = 0; i < MAX_CL; ++i) {
        if (pv_list[i] == NULL) {
            continue;
        } else {
            if (!strcmp(pv_list[i]->person1, name)) {
                if (!strcmp(pv_list[i]->person2, cli->name)) {
                    return 2;
                }
            }
        }
    }
    for (i = 0; i < MAX_CL; i++) {
        if (clients[i]) {
            if (clients[i]->name == name) {
                return 1;
            }
        }
    }
    return 0;
}


void add_to_pv_list(char *name1, char *name2) {
    for (int i = 0; i < MAX_CL; ++i) {
        if (pv_list[i] == NULL) {
            strcpy((pv_list[i])->person1, name1);
            strcpy((pv_list[i])->person2, name2);
        }
    }
}

void check_rcv(client_t *cli, char recv_buf[]) {

printf("in check_rcv\n" );
fflush(stdout);

    char check_val[CHK_VAL_SIZE];
    char pname[NAME_LENGHT];
    char port_str[6];
    int i = 0, k=0;
    int lenght_recv = strlen(recv_buf);
    bzero(port_str, 6);

    for (int k = 0; recv_buf[k] != ' ' || recv_buf[k] != '\0'; ++k) {
        check_val[i++] = recv_buf[k];
    }
    check_val[k] = '\0';
    if (strcmp(check_val, "name:") == 0) {
        int j = 0;
        for (i = 6; i < lenght_recv - 1; i++)
            pname[j++] = recv_buf[i];
        pname[j] = '\0';
        int found = name_found(cli, pname);

        if (found == 1) {
            add_to_pv_list(cli->name, pname);
            send(cli->sockfd, "FOUND", strlen("FOUND"), 0);
        } else if (found == 0) {
            send(cli->sockfd, "NOTFOUND", strlen("NOTFOUND"), 0);
        } else if (found == 2) {
            send(cli->sockfd, "CHAT", strlen("CHAT"), 0);
        }

    } else if (strcmp(check_val, "file:") == 0) {

    } else if (strcmp(check_val, "group:") == 0) {

    } else if (strcmp(check_val, "myPort") == 0) {
        bzero(port_str, 6);
        sprintf(port_str, "%d", cli->address.sin_port);
        send(cli->sockfd, port_str, strlen(port_str), 0);
    } else if (strcmp(check_val, "itsPort: ") == 0) {
        int j = 0;
        for (i = 6; i < lenght_recv - 1; i++)
            pname[j++] = recv_buf[i];
        pname[j] = '\0';
        for (i = 0; i < MAX_CL; i++) {
            if (clients[i]) {
                if (clients[i]->name == pname) {
                    bzero(port_str, 6);
                    sprintf(port_str, "%d", clients[i]->address.sin_port);
                }
            }
        }
        send(cli->sockfd, port_str, strlen(port_str), 0);
    } else {

    }

printf("check_rcv finished\n" );
fflush(stdout);

}

void *handling_clients(void *arg) {
    char sent_buf[BUF_SIZE];
    char name[NAME_LENGHT];
    char recv_buf[RCV_SIZE];
    int check_flag = 0;

    client_counter++;
    client_t *cli = (client_t *) arg;

    // Checking client's name
    if (recv(cli->sockfd, name, NAME_LENGHT, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1) {
        printf("Unacceptable name.\n");
        check_flag = 1;
    } else {
        strcpy(cli->name, name);
        sprintf(sent_buf, "%s has joined! \n", cli->name);
        printf("%s", sent_buf);
        send_msg(sent_buf, cli->user_id);
    }

    bzero(sent_buf, BUF_SIZE);

    while (1) {
        if (check_flag == 1) {
            break;
        }
        int receive = recv(cli->sockfd, recv_buf, BUF_SIZE, 0);
        check_rcv(cli, recv_buf);
        // In this condition everything is ok!

//        if (receive > 0) {
//            if (strlen(sent_buf) > 0) {
//                send_msg(sent_buf, cli->user_id);
//
//                str_triming(sent_buf, strlen(sent_buf));
//                printf("%s --> %s\n", sent_buf, cli->name);
//            }
//        }
            // If cleint wants to exit!
        if (receive == 0 || strcmp(sent_buf, "exit") == 0) {
            sprintf(sent_buf, "%s left\n", cli->name);
            printf("%s", sent_buf);
            send_msg(sent_buf, cli->user_id);
            check_flag = 1;
        }
            // If an error accurs!
        else {
            printf("ERROR: -1\n");
            check_flag = 1;
        }

        bzero(sent_buf, BUF_SIZE);
    }


    close(cli->sockfd);
    remove_from_queue(cli->user_id);
    free(cli);
    client_counter--;
    pthread_detach(pthread_self());

    return NULL;

}

int main() {

    //null pv_list
    for (int i = 0; i < MAX_CL; ++i) {
        //pv_list[i] = (pv_data *) malloc (MAX_CL * sizeof(pv_data));
        //bzero((pv_list[i])->person1, NAME_LENGHT);
        //bzero((pv_list[i])->person2, NAME_LENGHT);
        pv_list[i] = NULL;
    }


    // Defining ip and sin_por
    char *ip = "127.0.0.1"; // local ip
    int port = 8080;

    int option = 1;
    int listenfd = 0, connfd = 0;


    struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;
    pthread_t tid;



    //Sockent initialize
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);


    /* Ignore pipe signals */
    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char *) &option, sizeof(option)) < 0) {
        perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
    }

    // Binding
    if (bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }

    // Listening
    if (listen(listenfd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
    }

    printf("=== WELCOME TO THE CHAT ===\n");


    while (1) {
        socklen_t clilen = sizeof(client_addr);
        connfd = accept(listenfd, (struct sockaddr *) &client_addr, &clilen);


        // Check for Max cleints
        if ((client_counter + 1) == MAX_CL) {
            printf("Reached the Max of clients. Connection Rejected:");
            print_client_addr(client_addr);
            printf(":%d\n", client_addr.sin_port);
            close(connfd);
            continue;
        }


        // Cleints settings
        client_t *cli = (client_t *) malloc(sizeof(client_t));
        cli->address = client_addr;
        cli->sockfd = connfd;
        cli->user_id = user_id++;



        // Add to queue
        add_to_queue(cli);
        pthread_create(&tid, NULL, &handling_clients, (void *) cli);


        // Decrease CPU Usage
        sleep(1);
    }
    return EXIT_FAILURE;
}
