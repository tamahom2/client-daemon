#define _BSD_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include <endian.h>
#include <fcntl.h>
#include "tasks.h"

int is_kth_bit_one(long long int n, int k)
{
    if (n & (1 << k))
        return 1;
    return 0;
}

uint64_t get_last_id(tasklist *head)
{
    if (head == NULL)
        return 0;
    return (head->cur_id) + 1;
}

int get_length(tasklist *head)
{
    if (head == NULL)
        return 0;
    tasklist *tmp = head;
    int len = 0;
    while (tmp != NULL)
    {
        len += 1;
        tmp = tmp->next;
    }
    free(tmp);
    return len;
}
tasklist *add_task(tasklist *head, task *t)
{
    tasklist *temp = (tasklist *)malloc(sizeof(tasklist));
    temp->nbrun = 0;
    temp->cur_id = get_last_id(head);
    t->taskid = temp->cur_id;
    temp->cur = t;

    temp->next = head;
    head = temp;

    // Must create the file <id> in /runs
    char name[21];
    sprintf(name, "%lu", temp->cur_id);
    int fd = open(my_strcat("runs/task_id_", name), O_RDWR | O_CREAT | O_APPEND, 0777);
    close(fd);
    return head;
}

int remove_task(tasklist **head_ref, uint64_t id)
{
    tasklist *tmp = *head_ref;
    tasklist *prev = NULL;
    if (tmp != NULL && tmp->cur_id == id)
    {
        *head_ref = tmp->next;
        free(tmp);
        return 0;
    }
    while (tmp != NULL && tmp->cur_id != id)
    {
        prev = tmp;
        tmp = tmp->next;
    }
    if (tmp == NULL)
    {
        return -1;
    }
    prev->next = tmp->next;

    free(tmp);
    return 0;
}

char **get_commands(commandline *cmd)
{
    // get commands from commandline
    uint32_t argc = cmd->argc;
    char **argv = malloc((argc + 1) * sizeof(char *));
    for (int i = 0; i < argc; i++)
    {   
        argv[i] = malloc(cmd->argv[i]->len+1);
        printf("COMMAND IS %s OF LENGTH %d\n", cmd->argv[i]->text,cmd->argv[i]->len);
        strcpy(argv[i], cmd->argv[i]->text);
        argv[i][cmd->argv[i]->len] = NULL;
    }
    argv[argc] = NULL;
    return argv;
}

int execute_task(task *t)
{
    // Get current time to check if task is gonna be executed
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if (is_kth_bit_one(t->time->minutes, timeinfo->tm_min) && is_kth_bit_one(t->time->hours, timeinfo->tm_hour) && is_kth_bit_one(t->time->daysofweek, timeinfo->tm_wday))
    {
        // Execute the task and write the time in the <id>.txt
        char name[21];
        sprintf(name, "%lu", t->taskid);
        int fd = open(my_strcat("runs/task_id_", name), O_WRONLY|O_APPEND );
        printf("NAME IS %lu\n",t->taskid);
        if (fd == -1)
            return -1;
        // writing time
        uint64_t time_since_epoch = mktime(timeinfo);
        write(fd, &time_since_epoch, 8);
        // Getting the commandline
        int pid = fork();
        int status;
        if (pid == 0)
        {
            char **cmd = get_commands(t->command);
            int out_fd = open(my_strcat("runs/task_id_out_", name), O_WRONLY | O_CREAT | O_TRUNC, 0777);
            int err_fd = open(my_strcat("runs/task_id_err_", name), O_WRONLY | O_CREAT | O_TRUNC, 0777);
            dup2(out_fd, 1);
            dup2(err_fd, 2);
            execvp(cmd[0], cmd);
            free(cmd);
            close(out_fd);
            close(err_fd);
            /*int fd2 = open("/dev/null", O_WRONLY);
            dup2(fd2, 1);
            dup2(fd2, 2);
            close(fd2);*/
            exit(0);
        }
        else if (pid == -1)
            return -1;
        else
        {
            wait(pid, &status, 0);
            uint16_t exitcode = 65535;
            if(status<65535) exitcode = status;
            write(fd, &exitcode, 2);
        }
        close(fd);
        return 1;
    }
    return 0;
}

// Get nbrun if 0 Never run before if -1 Not found else number of runs
uint32_t get_nbrun(tasklist *head, uint64_t taskid)
{
    if (head == NULL)
        return -1;
    tasklist *tmp = head;
    while (tmp != NULL && tmp->cur_id != taskid)
    {
        tmp = tmp->next;
    }
    if (tmp == NULL)
    {
        return -1;
    }
    return tmp->nbrun;
}

int write_exit_codes(int fd, uint64_t taskid, uint32_t nbruns)
{
    // Open the <id> file;
    char name[21];
    sprintf(name, "%lu", taskid);
    int fd1 = open(my_strcat("runs/task_id_", name), O_RDONLY);
    for(int i =0;i<nbruns;i++)
    {
        uint64_t epoch;
        read(fd1, &epoch, 8);
        uint16_t exitcode;
        read(fd1, &exitcode, 2);
        //printf("Epoch : %lu, Exitcode : %d\n",epoch,exitcode);
        SwapBytes(&epoch, 8);
        SwapBytes(&exitcode, 2);
        write(fd, &epoch, 8);
        write(fd, &exitcode, 2);
    }
    close(fd1);
    return 0;
}
int execute_all_tasks(tasklist *head)
{
    if (head == NULL)
        return 0;
    tasklist *tmp = head;
    while (tmp != NULL)
    {
        printf("Executing task %lu\n", tmp->cur_id);
        int reply = execute_task(tmp->cur);
        if (reply == -1)
            return -1;
        if (reply)
            tmp->nbrun++;
        tmp = tmp->next;
    }
    free(tmp);
    return 0;
}

// You can either write in the pipe or in the file
int write_tasks(int fd, tasklist *head)
{
    tasklist *tmp = head;
    task *cur;
    while (tmp != NULL)
    {
        cur = tmp->cur;
        write_file(fd, cur);
        tmp = tmp->next;
    }
    free(tmp);
    return 0;
}
int list_tasks(int fd, tasklist *head)
{
    tasklist *tmp = head;
    task *cur_task;
    while (tmp != NULL)
    {
        cur_task = tmp->cur;
        uint64_t id = tmp->cur->taskid;
        SwapBytes(&id, 8);
        write(fd, &id, 8);
        write_task(fd, cur_task);
        tmp = tmp->next;
    }
    free(tmp);
    return 0;
}

// read_tasks from file
tasklist *read_tasks(int fd, tasklist *head)
{
    uint32_t nbtasks;
    int r = lseek(fd, 0, SEEK_END); // goto end of file
    if (r == 0)
    {
        return NULL;
    }
    lseek(fd, 0, SEEK_SET);
    if (read(fd, &nbtasks, 4) < 0)
    {
        return NULL;
    }
    for (int i = 0; i < nbtasks; i++)
    {
        task *t = read_task_file(fd);
        head = add_task(head, t);
    }
    return head;
}
void read_task_pipe(int fd, task *t)
{
    t->command = malloc(sizeof(commandline));
    t->time = malloc(sizeof(struct timing));
    read(fd, &t->time->minutes, 8);
    SwapBytes(&t->time->minutes, 8);

    read(fd, &t->time->hours, 4);
    SwapBytes(&t->time->hours, 4);
    read(fd, &t->time->daysofweek, 1);
    read(fd, &t->command->argc, 4);
    SwapBytes(&t->command->argc, 4);
    t->command->argv = malloc(t->command->argc * sizeof(str));
    for (int i = 0; i < t->command->argc; i++)
    {
        t->command->argv[i] = malloc(sizeof(str));
        read(fd, &t->command->argv[i]->len, 4);
        SwapBytes(&t->command->argv[i]->len, 4);
        t->command->argv[i]->text = malloc(t->command->argv[i]->len+1);
        read(fd, t->command->argv[i]->text, t->command->argv[i]->len);
    }
}

task *read_task_file(int fd)
{
    task *t = malloc(sizeof(task));
    t->command = malloc(sizeof(commandline));
    t->time = malloc(sizeof(struct timing));
    read(fd, &t->time->minutes, 8);
    read(fd, &t->time->hours, 4);
    read(fd, &t->time->daysofweek, 1);
    read(fd, &t->command->argc, 4);
    t->command->argv = malloc(t->command->argc * sizeof(str));
    for (int i = 0; i < t->command->argc; i++)
    {
        t->command->argv[i] = malloc(sizeof(str));
        read(fd, &t->command->argv[i]->len, 4);
        t->command->argv[i]->text = malloc(t->command->argv[i]->len);
        read(fd, t->command->argv[i]->text, t->command->argv[i]->len);
    }
    return t;
}

int write_file(int fd, task *t)
{
    int w = write(fd, &t->time->minutes, 8);
    w = write(fd, &t->time->hours, 4);
    w = write(fd, &t->time->daysofweek, 1);
    int num = t->command->argc;
    w = write(fd, &num, 4);
    for (uint32_t i = 0; i < t->command->argc; i++)
    {
        uint32_t sz = t->command->argv[i]->len;
        w = write(fd, &sz, 4);
        w = write(fd, t->command->argv[i]->text, strlen(t->command->argv[i]->text));
    }
    return 0;
}
