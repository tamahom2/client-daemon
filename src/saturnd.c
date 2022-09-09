#define _XOPEN_SOURCE 700
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include "common.h"
#include <fcntl.h>
#include "tasks.h"

volatile sig_atomic_t signalrecu = 0; // pour empêcher le compilateur d'optimiser et l'informer que le système peut changer cette variable sans prévenir

// Procédure de terminaison
void reception(int signum)
{
    if (signum == SIGTERM)
    {
        signalrecu = 1;
    }
}

// Fonction d'initialisation du daemon
/*static */ void start_daemon()
{
    pid_t pid_fork1, pid_fork2;
    int capt;
    struct sigaction terminaison;
    memset(&terminaison, 0, sizeof(struct sigaction));
    // premier fork
    pid_fork1 = fork();
    if (pid_fork1 < 0)
    {
        perror("le premier fork a échoué");
        exit(EXIT_FAILURE);
    }
    if (pid_fork1 > 0) // on termine le père
    {
        _exit(EXIT_SUCCESS);
    }
    if (setsid() < 0) // vérifier que le fils est session leader
    {                 // sert à se défaire du contrôle du terminal
        perror("le fils du premier fork n'est pas le session leader");
        exit(EXIT_FAILURE);
    }

    // Prise en charge de la terminaison
    terminaison.sa_handler = &reception;
    sigfillset(&terminaison.sa_mask);
    terminaison.sa_flags = SA_RESTART;
    capt = sigaction(SIGTERM, &terminaison, NULL);
    if (capt < 0)
    {
        perror("la mise en place de l'handler a échoué");
        exit(EXIT_FAILURE);
    }

    // second fork
    pid_fork2 = fork();
    if (pid_fork2 < 0)
    {
        perror("le second fork a échoué");
        exit(EXIT_FAILURE);
    }
    if (pid_fork2 > 0)
    {
        _exit(EXIT_SUCCESS); // il ne reste plus que le petit-fils
    }
    umask(0);
}

int main(int argc, char *argv[])
{
    start_daemon();
    // création du tube de requête
    mkdir_p(PIPES_DIR);
    mkdir_p("./runs");
    char *request_pipe = my_strcat(PIPES_DIR, "/saturnd-request-pipe");
    char *reply_pipe = my_strcat(PIPES_DIR, "/saturnd-reply-pipe");
    mkfifo(request_pipe, 0666);
    mkfifo(reply_pipe, 0666);

    // accès aux pipes (lecture sur request, écriture sur reply)
    int fd_request = open(request_pipe, O_RDWR | O_NONBLOCK); // lecture non-bloquante des requêtes
    if (fd_request == -1)
    {
        perror("erreur lors de l'ouverture du tube des requêtes");
        exit(EXIT_FAILURE);
    }
    int fd_reply = open(reply_pipe, O_RDWR | O_NONBLOCK); // écriture non-bloquante des réponses
    if (fd_reply == -1)
    {
        perror("erreur lors de l'ouverture du tube des réponses");
        exit(EXIT_FAILURE);
    }
    // main loop
    /*struct pollfd fds[2];
    fds[0].fd = fd_request;
    fds[0].events = POLLOUT;
    fds[1].fd = fd_reply;
    fds[1].events = POLLOUT;*/
    fd_set fds;

    FD_ZERO(&fds); // Clear FD set for select
    FD_SET(fd_request, &fds);
    // Lire et ajouter les taches
    tasklist *head = NULL;
    int fd_logs = open("logs", O_RDONLY | O_CREAT, 0777);
    head = read_tasks(fd_logs, head);
    close(fd_logs);
    int stop = 0;
    while (!stop)

    {
        execute_all_tasks(head);
        if (signalrecu == 1)
        {
            signalrecu = 0;
            stop = 1;
            break;
            // appeler les fonctions que l'on veut ici, pas besoin d'être en signal-safety
        }

        // [TODO] il faut itérer sur toute la table des tâches voir si'il ya
        // taches à executer et les executer - ism

        // [TODO] surveiller s'il ya des fds prêt pour l'ecriture de la aprt de cassini fd_request
        // à l'aide d'un select - chems
        int check = select(fd_request + 1, &fds, NULL, NULL, NULL);
        if (check < 0)
        {
            printf("Select error\n");
            // Safe exit
            stop = 1;
            break;
        }
        if (check > 0 && FD_ISSET(fd_request, &fds))
        {

            uint16_t rep;
            read(fd_request, &rep, 2);
            SwapBytes(&rep, 2);
            if (rep == CLIENT_REQUEST_CREATE_TASK)
            {
                task *t = malloc(sizeof(task));
                read_task_pipe(fd_request, t);
                head = add_task(head, t);
                uint64_t id = head->cur_id;
                uint16_t reply = SERVER_REPLY_OK;
                SwapBytes(&reply, 2);
                SwapBytes(&id, 8);
                write(fd_reply, &reply, 2);
                write(fd_reply, &id, 8);
            }
            if (rep == CLIENT_REQUEST_GET_TIMES_AND_EXITCODES)
            {
                uint64_t task_id;
                read(fd_request, &task_id, 8);
                SwapBytes(&task_id, 8);
                //printf("THE TASK ID IS %lu\n", task_id);
                uint32_t nbruns = get_nbrun(head, task_id);
                uint16_t reply;
                if (nbruns == 0)
                {
                    reply = SERVER_REPLY_ERROR;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                    reply = SERVER_REPLY_ERROR_NEVER_RUN;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                }
                else if (nbruns == -1)
                {
                    reply = SERVER_REPLY_ERROR;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                    reply = SERVER_REPLY_ERROR_NOT_FOUND;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                }
                else
                {
                    reply = SERVER_REPLY_OK;
                    SwapBytes(&reply, 2);
                    //printf("%s\n", (char *)&reply);
                    write(fd_reply, &reply, 2);
                    //printf("NBRUNS ARE : %d\n", nbruns);
                    SwapBytes(&nbruns, 4);
                    //printf("NBRUNS AFTER : %d\n", nbruns);
                    if (write(fd_reply, &nbruns, 4) < 0)
                        printf("ERROR\n");
                    SwapBytes(&nbruns, 4);
                    write_exit_codes(fd_reply, task_id, nbruns);
                }
            }
            if (rep == CLIENT_REQUEST_REMOVE_TASK)
            {
                uint64_t task_id;
                read(fd_request, &task_id, sizeof(uint64_t));
                SwapBytes(&task_id, sizeof(uint64_t));
                uint16_t reply;
                if (remove_task(&head, task_id) == 0)
                {
                    reply = SERVER_REPLY_OK;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                }
                else
                {
                    reply = SERVER_REPLY_ERROR;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                    reply = SERVER_REPLY_ERROR_NOT_FOUND;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                }
            }
            if (rep == CLIENT_REQUEST_GET_STDERR)
            {
                // afficher la sortie erreur standard de la dernière exécution de la tâche
                uint64_t task_id;
                read(fd_request, &task_id, sizeof(uint64_t));
                SwapBytes(&task_id, sizeof(uint64_t));
                uint32_t nbruns = get_nbrun(head, task_id);
                uint16_t reply;
                if (nbruns == 0)
                {
                    reply = SERVER_REPLY_ERROR;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                    reply = SERVER_REPLY_ERROR_NEVER_RUN;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                }
                else if (nbruns == -1)
                {
                    reply = SERVER_REPLY_ERROR;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                    reply = SERVER_REPLY_ERROR_NOT_FOUND;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                }
                else
                {
                    uint16_t reply = SERVER_REPLY_OK;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                    char name[21];
                    sprintf(name, "%lu", task_id);
                    int fd_err = open(my_strcat("runs/task_id_err_", name), O_RDONLY);
                    file_to_stdout(fd_err, fd_reply);
                    close(fd_err);
                }
            }
            if (rep == CLIENT_REQUEST_GET_STDOUT)
            {
                // afficher la sortie standard de la dernière exécution de la tâche
                uint64_t task_id;
                read(fd_request, &task_id, sizeof(uint64_t));
                SwapBytes(&task_id, sizeof(uint64_t));
                uint32_t nbruns = get_nbrun(head, task_id);
                printf("NBRUNS IS : %d\n", nbruns);
                uint16_t reply;
                if (nbruns == 0)
                {
                    reply = SERVER_REPLY_ERROR;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                    reply = SERVER_REPLY_ERROR_NEVER_RUN;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                }
                else if (nbruns == -1)
                {
                    reply = SERVER_REPLY_ERROR;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                    reply = SERVER_REPLY_ERROR_NOT_FOUND;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                }
                else
                {
                    uint16_t reply = SERVER_REPLY_OK;
                    SwapBytes(&reply, 2);
                    write(fd_reply, &reply, 2);
                    char name[21];
                    sprintf(name, "%lu", task_id);
                    int fd_out = open(my_strcat("runs/task_id_out_", name), O_RDONLY);
                    printf("NOW HERE \n");
                    file_to_stdout(fd_out, fd_reply);
                    close(fd_out);
                }
            }
            if (rep == CLIENT_REQUEST_TERMINATE)
            {
                uint16_t reply = SERVER_REPLY_OK;
                SwapBytes(&reply, 2);
                write(fd_reply, &reply, 2);
                stop = 1;
                break;
            }
            if (rep == CLIENT_REQUEST_LIST_TASKS)
            {
                // lister toutes les tâches
                uint16_t reply = SERVER_REPLY_OK;
                SwapBytes(&reply, 2);
                write(fd_reply, &reply, 2);
                uint32_t nbtasks = get_length(head);
                SwapBytes(&nbtasks, 4);
                write(fd_reply, &nbtasks, 4);
                list_tasks(fd_reply, head); // writes tasks into the pipe
            }

            // il y a un changement dans la pipes
        }
        sleep(10);
        // sleep(45);
    }

    // Ecrire les taches dans un fichier lors du terminate
    fd_logs = open("logs", O_WRONLY | O_CREAT | O_TRUNC,0777);
    uint32_t nbtasks = get_length(head);
    write(fd_logs, &nbtasks, 4);
    write_tasks(fd_logs, head);
    free(head);
    close(fd_reply);
    close(fd_logs);
    close(fd_request);
    return 0;
}
