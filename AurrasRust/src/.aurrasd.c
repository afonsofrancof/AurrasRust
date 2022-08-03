#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>

#define tamanho_filtros_array 10
#define MAX_FILTROS_USER 10
#define Client_Server_Main "../etc/client_server_main_fifo"


//Variaveis globais a serem alteradas pela main e pelo SIGHandler.
char *array_conf[30];
int max_filters[tamanho_filtros_array] = {0};
int filtros_em_uso[tamanho_filtros_array] = {0};
int n_filtros;
typedef struct espera {
    pid_t pid;
    char comando[150];
    int filtros[tamanho_filtros_array];
    struct espera *prox;
} *ESPERA;
ESPERA main_queue = NULL;
ESPERA main_executing = NULL;

ESPERA remove_executing(int pid) {
    ESPERA a = NULL, b;
    if (main_executing->prox == NULL) {
        a = main_executing;
        main_executing = NULL;
        return a;
    }
    for (b = main_executing; b && b->prox; b = b->prox) {
        if (b->prox->pid == pid) {
            a = b->prox;
            b->prox = a->prox;
            a->prox = NULL;
        }
    }
    return a;
}

void sigvoid() {}

int parse_string_to_string_array(char *string, char **str_array, char *delimit) {
    //Dar parse a uma string num array de char pointers.
    int n_words = 0;
    char *p = strtok(string, delimit);
    while (p != NULL) {
        str_array[n_words++] = p;
        p = strtok(NULL, delimit);
    }
    return n_words;
}


ESPERA adiciona_cauda(ESPERA lista, ESPERA b) {

    if (lista == NULL) lista = b;
    else {
        ESPERA a = lista;
        while (a->prox != NULL) {
            a = a->prox;
        }
        a->prox = b;
    }
    return lista;
}


void sigHandler(int signum) {
    char pid_filho_string[15] = {0};

    int fifo_handler = open("../tmp/fifo_sighandler", O_RDONLY);
    read(fifo_handler, pid_filho_string, sizeof(pid_filho_string));
    close(fifo_handler);
    int pid_filho = atoi(pid_filho_string);
    printf("%d\n", pid_filho);
    printf("PID %d ACABOU\n", pid_filho);
    ESPERA removido = remove_executing(pid_filho);
    for (int i = 0; i < n_filtros; i++) {
        filtros_em_uso[i] -= removido->filtros[i];
    }


    while (main_queue) {
        for (int i = 0; i < n_filtros; i++) {
            if (filtros_em_uso[i] + main_queue->filtros[i] > max_filters[i]) return;
        }
        ESPERA a = main_queue;
        main_queue = main_queue->prox;
        a->prox = NULL;
        main_executing = adiciona_cauda(main_executing, a);
        int pid_prox = a->pid;

        for (int i = 0; i < n_filtros; i++) {
            filtros_em_uso[i] += a->filtros[i];
        }
        printf("%d mandado executar\n", pid_prox);
        kill(pid_prox, SIGUSR2);
    }
    /* for (int i = 0; i < n_filtros; i++) {
         filtros_em_uso[i] += main_queue
     }*/
}


int main(int argc, char **argv) {

    mkfifo("../tmp/fifo_sighandler", 0666);

    signal(SIGUSR1, sigHandler);
    switch (argc) {
        case 1:
            printf("No arguments provided\n");
            return -1;
        case 2:
            printf("Too few arguments in program call\n");
            return -1;
        case 3:
            break;
        default:
            printf("Too many arguments in program call\n");
            return -1;
    }
    //Abrir ficheiro de config e dar parse para um array de strings;
    char conf_buf[BUFSIZ];
    int conf_fd = open(argv[1], O_RDONLY);
    //Ler esse tal ficheiro
    read(conf_fd, conf_buf, sizeof(conf_buf));
    close(conf_fd);
    //Dar parse do ficheiro de config para um array de char pointers.
    int size_array_conf = parse_string_to_string_array(conf_buf, array_conf, " \n");
    //Definir o numero de filtros (variavel global *Importante*) para uso varias vezes ao longo do programa.
    n_filtros = size_array_conf / 3;
    for (int i = 2, u = 0; u < n_filtros; i = i + 3, u++) {
        max_filters[u] = atoi(array_conf[i]);
    }

    // Parte 1 - Diretoria temporaria para os pipes com nome (FIFO) para comunicacao privada entre cliente e servidor.
    char client_server_path[200] = "../tmp/client_server_";
    char server_client_path[200] = "../tmp/server_client_";

    // Criar o pipe principal.
    mkfifo(Client_Server_Main, 0666);
    printf("Server ON.\n");

    //Abrir o main pipe.
    int fd_client_server_main;
    fd_client_server_main = open(Client_Server_Main, O_RDONLY);

    int pipe_filho_main[2];
    if (pipe(pipe_filho_main) == -1) printf("Erro ao criar pipe");
    //Este loop serve para podermos ter mais que 1 processo em execucao simultanea.
    while (1) {

        // Abrir o main pipe e ficar à espera que os users se conectem (escrevam o seu "pid;comando").
        char buf[BUFSIZ];
        ssize_t tamanho_read;
        tamanho_read = read(fd_client_server_main, buf, sizeof(buf));
        //Se o que lermos do main pipe for maior que 0, ou seja, alguem se conectou e escreveu o seu pid;comando , continuamos.

        if (tamanho_read > 0) {

            char *main_response[2] = {0};
            int main_response_size = parse_string_to_string_array(buf, main_response, ";");
            //CHECK se tem mais 1 ; na frase , para protecao
            char pid_client[11];
            strcpy(pid_client, main_response[0]);

            /*
            * Concatenamos o que lemos da mainpipepe[0] (o pid do processo que se conectou ao server) com a path da
            diretoria onde sera guardado o private pipe do cliente com o server.
            */
            char Server_Client_ID[BUFSIZ];
            char Client_Server_ID[BUFSIZ];
            strcpy(Server_Client_ID, server_client_path);
            strcpy(Client_Server_ID, client_server_path);
            strcat(Server_Client_ID, pid_client);
            strcat(Client_Server_ID, pid_client);

            //Criar os fifos nessa diretoria.
            if (mkfifo(Server_Client_ID, 0666) == -1)
                printf("Erro creating FIFO\n");
            if (mkfifo(Client_Server_ID, 0666) == -1)
                printf("Erro creating FIFO\n");

            //Abrir os fifos para comunicacoa
            int fd_server_client_id = open(Server_Client_ID, O_WRONLY);
            int fd_client_server_id = open(Client_Server_ID, O_RDONLY);

            //Reportar de volta para o cliente que a coneccao sucedeu.
            char *con = "Connection Accepted";
            if (write(fd_server_client_id, con, 20) < 0) {
                perror("Erro na escrita:");
                //_exit(-1);
            }

            //Dividir o comando do user num array de char pointers.
            char *response_array[30] = {0};
            char comando[200];
            strcpy(comando, main_response[1]);
            int response_c = parse_string_to_string_array(main_response[1], response_array, " \0");
            //FALTA AQUI VER SE RESPONSE C TA VAZIO NO CASO DE NAO RECEBER NADA
            int n_comandos = response_c - 3;
            int continua = 1;
            int filtros_cliente[10] = {0};
            int status_flag = 0;
            if (strcmp(response_array[0], "status") == 0) {
                if (response_c > 1) {
                    printf("Too many arguments in program call\n");
                } else {
                    status_flag = 1;
                    ESPERA a = main_queue;
                    ESPERA b = main_executing;
                    char status[BUFSIZ] = {0};
                    strcat(status, "Running tasks:\n");
                    while (b) {
                        strcat(status, "\t");
                        char pid_status[11] = {0};
                        sprintf(pid_status, "%d", b->pid);
                        strcat(status, pid_status);
                        strcat(status, " - ");
                        strcat(status, b->comando);
                        strcat(status, "\n");
                        b = b->prox;
                    }
                    strcat(status, "Pending tasks:\n");
                    while (a) {
                        strcat(status, "\t");
                        char pid_status2[11] = {0};
                        sprintf(pid_status2, "%d", a->pid);
                        strcat(status, pid_status2);
                        strcat(status, " - ");
                        strcat(status, a->comando);
                        strcat(status, "\n");
                        a = a->prox;
                    }
                    strcat(status, "Filter status:\n");
                    for (int i = 0, u = 0; i < n_filtros; i++, u = u + 3) {
                        strcat(status, "\t");
                        strcat(status, "filtro ");
                        strcat(status, array_conf[u]);
                        char filtro_status3[40] = {0};
                        sprintf(filtro_status3, ": %d/", filtros_em_uso[i]);
                        strcat(status, filtro_status3);
                        sprintf(filtro_status3, "%d (running/max)\n", max_filters[i]);
                        strcat(status, filtro_status3);
                    }
                    write(fd_server_client_id, status, sizeof(status));
                }
            } else if (strcmp(response_array[0], "transform") == 0) {
                if (response_c < 4) {
                    printf("Not enough arguments in program call\n");
                } else {
                    /*
                    * Substituir os filtros dados pelo user pelo nome do ficheiro correspondente
                    * tendo em conta o ficheiro de config e adicionar os filtros que o user quer
                    * usar num novo array local.
                    */

                    for (int i = 3; i < response_c; i++) {
                        for (int u = 0, l = 0; u < size_array_conf; u = u + 3, l++) {
                            if (strcmp(response_array[i], array_conf[u]) == 0) {
                                response_array[i] = array_conf[u + 1];
                                filtros_cliente[u / 3]++;
                                break;
                            }
                        }
                    }

                    if (main_queue != NULL) { continua = 0; }
                    for (int i = 0; i < n_filtros; i++) {
                        if (filtros_em_uso[i] + filtros_cliente[i] > max_filters[i]) continua = 0;
                    }
                    if (continua == 1) {
                        for (int i = 0; i < n_filtros; i++) {
                            filtros_em_uso[i] += filtros_cliente[i];
                        }
                    }

                }
            }
            pid_t pid;
            if (status_flag == 0) {
                //Nesta parte criamos o main Filho. Este filho ira representar um cliente.
                if ((pid = fork()) == 0) {
                    signal(SIGUSR2, sigvoid);
                    printf("Continua %d | Process %d\n", continua, getpid());
                    if (continua == 0) {
                        pause();
                        printf("OI\n");
                    }
                    char path[BUFSIZ] = {0};
                    strcat(path, argv[2]);
                    strcat(path, "/"); //MUDAR
                    //puts(path);

                    if (n_comandos == 1) {
                        if (fork() == 0) {

                            int fd_r = open(response_array[1], O_RDONLY);
                            int fd_w = open(response_array[2], O_CREAT | O_WRONLY, 0666);
                            dup2(fd_r, 0);
                            close(fd_r);
                            dup2(fd_w, 1);
                            close(fd_w);
                            strcat(path, response_array[3]);
                            execl(path, response_array[3], NULL);
                            _exit(0);
                        }
                    } else {
                        int pd[n_comandos][2];
                        for (int i = 0; i < n_comandos; i++) {
                            if (pipe(pd[i]) == -1) {
                                printf("Pipe Error\n");
                                _exit(-1);
                            }
                            if (i == 0) {
                                if (fork() == 0) {
                                    close(pd[i][0]);
                                    int fd = open(response_array[1], O_RDONLY);
                                    dup2(fd, 0);
                                    close(fd);
                                    dup2(pd[i][1], 1);
                                    close(pd[i][1]);
                                    //FALTA IR BUSCAR AO ARGV O FILTER PATH
                                    strcat(path, response_array[3]);
                                    execl(path, path, (char *) NULL);
                                    _exit(12);
                                } else {
                                    close(pd[i][1]);
                                }
                            } else if (i == n_comandos - 1) {
                                if (fork() == 0) {
                                    close(pd[i][0]);
                                    close(pd[i][1]);
                                    int fd = open(response_array[2], O_CREAT | O_WRONLY, 0666);
                                    dup2(fd, 1);
                                    close(fd);
                                    dup2(pd[i - 1][0], 0);
                                    close(pd[i - 1][0]);
                                    strcat(path, response_array[response_c - 1]);
                                    execl(path, path, (char *) NULL);
                                    _exit(12);
                                } else {
                                    close(pd[i][0]);
                                    close(pd[i][1]);
                                    close(pd[i - 1][0]);
                                    int status;
                                    wait(&status);
                                }

                            } else {
                                if (fork() == 0) {
                                    close(pd[i][0]);
                                    dup2(pd[i - 1][0], 0);
                                    dup2(pd[i][1], 1);
                                    close(pd[i - 1][0]);
                                    close(pd[i][1]);
                                    strcat(path, response_array[i + 3]);
                                    execl(path, path, (char *) NULL);
                                    _exit(12);
                                } else {
                                    close(pd[i][1]);
                                    close(pd[i - 1][0]);
                                }
                            }

                        }
                        for (int i = 0; i < n_comandos; i++) {
                            int status;
                            wait(&status);
                        }

                    }
                    kill(getppid(), SIGUSR1);
                    sleep(1);
                    int fifo_handler = open("../tmp/fifo_sighandler", O_WRONLY);
                    char my_id[15] = {0};
                    snprintf(my_id, sizeof(my_id), "%d", getpid());
                    write(fifo_handler, my_id, sizeof(my_id));
                    close(fifo_handler);



                    //memset(buf, 0, sizeof(buf));
                    _exit(0);
                } else if (pid != 0) {
                    //Criar nodo
                    ESPERA b = malloc(sizeof(struct espera));
                    b->pid = pid;
                    strcpy(b->comando, comando);
                    for (int i = 0; i < n_filtros; i++) {
                        b->filtros[i] = filtros_cliente[i];
                    }
                    b->prox = NULL;

                    //Adicionar o nodo na queue ou nos executing
                    if (continua == 0) {
                        main_queue = adiciona_cauda(main_queue, b);
                    } else {
                        main_executing = adiciona_cauda(main_executing, b);
                    }
                }
            }
            close(fd_server_client_id);
            close(fd_client_server_id);

            unlink(Server_Client_ID);
            unlink(Client_Server_ID);

        }//break;

    }
    close(fd_client_server_main);
    int status;
    wait(&status);
    sleep(3);
    printf("bomdia");

    unlink("../tmp/fifo_sighandler");
    unlink(Client_Server_Main);
    return 0;
}