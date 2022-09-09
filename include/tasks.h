#ifndef TASKS_H
#define TASKS_H

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "client-request.h"
#include "server-reply.h"
#include "timing.h"
#include "commandline.h"

typedef struct
{
    uint64_t taskid;
    struct timing *time;
    commandline *command;

} task;

typedef struct tasklist tasklist;

struct tasklist
{
    uint32_t nbrun;
    uint64_t cur_id;
    task *cur;
    tasklist *next;
};

int is_kth_bit_one(long long int n, int k);
uint64_t get_last_id(tasklist *head);
int get_length(tasklist *head);
tasklist *add_task(tasklist *head, task *t);
int remove_task(tasklist **head, uint64_t id);
char **get_commands(commandline *cmd);
int execute_task(task *t);
uint32_t get_nbrun(tasklist *head,uint64_t taskid);
int write_exit_codes(int fd,uint64_t taskid,uint32_t nbruns);
int execute_all_tasks(tasklist *head);
int write_tasks(int fd, tasklist *head);
tasklist *read_tasks(int fd, tasklist *head);
void read_task_pipe(int fd, task *t);
task *read_task_file(int fd);
int write_file(int fd, task *t);
#endif // TASKS
