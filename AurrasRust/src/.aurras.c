#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <string.h>

#define Client_Server_Main  "../etc/client_server_main_fifo"


int main(int argc, char **argv) {
    char Client_Server_ID[BUFSIZ] = "../tmp/client_server_";
    char Server_Client_ID[BUFSIZ] = "../tmp/server_client_";

    int fd_client_server_main;

    fd_client_server_main = open(Client_Server_Main, O_WRONLY);
    if (fd_client_server_main == -1) printf("error accessing fd_client_server_main");

    /* Pedir coneccao ao servidor e mandar o pedido*/
    char command[BUFSIZ];
    memset(command, 0, sizeof(command));
    snprintf(command, sizeof(command), "%d;", (int) getpid());
    int status = 0;
    for (int i = 1; i < argc; i++) {
        strcat(command, argv[i]);
        strcat(command, " ");
    }
    if (argc <= 1) {
        printf("No argument provided");
        return -1;
    } else {
        if (strcmp(argv[1], "transform") == 0) {//argv[5][2]
            if (argc < 5) {
                printf("Not enough arguments in program call");
                return -1;
            } else {
                if (write(fd_client_server_main, command, sizeof(command)) < 0) {
                    perror("Write:");//print error
                    exit(-1);
                }
            }
        } else if (strcmp(argv[1], "status") == 0) {
            if (argc > 2) {
                printf("Too many arguments in program call");
                return -1;
            } else {
                status = 1;
                if (write(fd_client_server_main, command, sizeof(command)) < 0) {
                    perror("Write:");//print error
                    exit(-1);
                }

            }
        }
    }

    //FALTA ARRANJAR UMA FORMA DE VER SE O SERVIDOR CONSEGUIU CRIAR O FIFO OU O FORK
    sleep(1);
    char pid[10];
    snprintf(pid, sizeof(pid), "%d", (int) getpid());
    strcat(Server_Client_ID, pid);
    int fd_server_client_id = open(Server_Client_ID, O_RDONLY);
    if (fd_server_client_id == -1) {
        printf("Error accessing private Server_Client_FIFO.Server out of memory or Max users connected.\n");
        return -1;
    }

    strcat(Client_Server_ID, pid);
    int fd_client_server_id = open(Client_Server_ID, O_WRONLY);
    if (fd_client_server_id == -1) {
        printf("Error accessing private Client_Server_FIFO.Server out of memory or Max users connected.\n");
        return -1;
    }
    char read_id[40] = {0};
    if (read(fd_server_client_id, read_id, sizeof(read_id)) < 0) {
        perror("Erro na Leitura:"); //error check
        exit(-1);
    }
    printf("\nServer Response: %s\n", read_id);

    //Checking what the user wants to do
    if (status == 1) {
        char read_status[BUFSIZ] = {0};
        if (read(fd_server_client_id, read_status, sizeof(read_status)) < 0) {
            perror("Erro na Leitura:"); //error check
            exit(-1);
        }
        printf("%s\n", read_status);
    }


    close(fd_client_server_main);
    close(fd_server_client_id);
    close(fd_client_server_id);


    /* remove the FIFO */

    return 0;
}