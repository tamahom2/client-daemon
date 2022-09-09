#define _BSD_SOURCE
#include <endian.h>
#include "cassini.h"
#include <stdint.h>
#include <string.h>
#include "timing.h"
#include "commandline.h"
#include "string.h"
#include <time.h>
#include <poll.h>
#include <sys/select.h>
#include <unistd.h>
#include "common.h"
#include "tasks.h"

#define MAX_TASK 100

const char usage_info[] = "\
   usage: cassini [OPTIONS] -l -> list all tasks\n\
      or: cassini [OPTIONS]    -> same\n\
      or: cassini [OPTIONS] -q -> terminate the daemon\n\
      or: cassini [OPTIONS] -c [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] COMMAND_NAME [ARG_1] ... [ARG_N]\n\
          -> add a new task and print its TASKID\n\
             format & semantics of the \"timing\" fields defined here:\n\
             https://pubs.opengroup.org/onlinepubs/9699919799/utilities/crontab.html\n\
             default value for each field is \"*\"\n\
      or: cassini [OPTIONS] -r TASKID -> remove a task\n\
      or: cassini [OPTIONS] -x TASKID -> get info (time + exit code) on all the past runs of a task\n\
      or: cassini [OPTIONS] -o TASKID -> get the standard output of the last run of a task\n\
      or: cassini [OPTIONS] -e TASKID -> get the standard error\n\
      or: cassini -h -> display this message\n\
\n\
   options:\n\
     -p PIPES_DIR -> look for the pipes in PIPES_DIR (default: /tmp/<USERNAME>/saturnd/pipes)\n\
";

void parse_argv(task *t, uint32_t ind, char **argv)
{
  // printf("the whole length is %ld\n", t->command->argc);

  for (uint32_t i = 0; i < t->command->argc; i++)
  {
    t->command->argv[i]->len = strlen(argv[ind + i + 1]);
    t->command->argv[i]->text = malloc(t->command->argv[i]->len+1);
    strcpy(t->command->argv[i]->text,argv[ind + i + 1]);
    //printf("the command is %s of length %ld\n",t->command->argv[i]->text,t->command->argv[i]->len );
  }
}

int main(int argc, char *argv[])
{
  errno = 0;

  char *minutes_str = "*";
  char *hours_str = "*";
  char *daysofweek_str = "*";
  char *pipes_directory = NULL;

  uint16_t operation = CLIENT_REQUEST_LIST_TASKS;
  uint64_t taskid;

  int opt;
  char *strtoull_endp;
  while ((opt = getopt(argc, argv, "hlcqm:H:d:p:r:x:o:e:")) != -1)
  {
    switch (opt)
    {
    case 'm':
      minutes_str = optarg;
      break;
    case 'H':
      hours_str = optarg;
      break;
    case 'd':
      daysofweek_str = optarg;
      break;
    case 'p':
      pipes_directory = optarg;
      if (pipes_directory == NULL)
        goto error;
      break;
    case 'l':
      operation = CLIENT_REQUEST_LIST_TASKS;
      break;
    case 'c':
      operation = CLIENT_REQUEST_CREATE_TASK;
      break;
    case 'q':
      operation = CLIENT_REQUEST_TERMINATE;
      break;
    case 'r':
      operation = CLIENT_REQUEST_REMOVE_TASK;
      taskid = strtoull(optarg, &strtoull_endp, 10);
      if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
        goto error;
      break;
    case 'x':
      operation = CLIENT_REQUEST_GET_TIMES_AND_EXITCODES;
      taskid = strtoull(optarg, &strtoull_endp, 10);
      if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
        goto error;
      break;
    case 'o':
      operation = CLIENT_REQUEST_GET_STDOUT;
      taskid = strtoull(optarg, &strtoull_endp, 10);
      if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
        goto error;
      break;
    case 'e':
      operation = CLIENT_REQUEST_GET_STDERR;
      taskid = strtoull(optarg, &strtoull_endp, 10);
      if (strtoull_endp == optarg || strtoull_endp[0] != '\0')
        goto error;
      break;
    case 'h':
      printf("%s", usage_info);
      return 0;
    case '?':
      fprintf(stderr, "%s", usage_info);
      goto error;
    }
  }

  // --------
  // | TODO |
  // --------
  make_daemon();
  sleep(5);
  // On change de uint16_t en char
  uint16_t c16 = operation;
  SwapBytes(&c16,2);
  // lire de request et afficher
  // on envoie la requête via le tube de requête
  // pipe ->  cette liste (via le tube de réponse) -> l'afficher

  int fd;
  /*char *username = malloc(20);
  getlogin_r(username, 20);*/
  char *mypipe = my_strcat((char *)PIPES_DIR, "/saturnd-request-pipe");
  if (pipes_directory != NULL)
  {
    mypipe = my_strcat(pipes_directory, "/saturnd-request-pipe");
  }
  fd = open(mypipe, O_WRONLY | O_NONBLOCK);
  if (fd == -1)
  {
    perror("error while openning request pipe");
    exit(1);
  }

  if (write(fd, &c16, 2) == -1)
  {
    perror("error while writing in the request pipe");
    exit(1);
  }
  
  if ((operation == CLIENT_REQUEST_REMOVE_TASK) || (operation == CLIENT_REQUEST_GET_TIMES_AND_EXITCODES) || (operation == CLIENT_REQUEST_GET_STDOUT) || (operation == CLIENT_REQUEST_GET_STDERR))
  {
    uint64_t tmp = taskid;
    SwapBytes(&tmp,8);
    if (write(fd, &tmp, 8) == -1)
    {
      perror("error while writing task id");
      exit(1);
    }
  }

  if (operation == CLIENT_REQUEST_CREATE_TASK)
  {
    // Add mallocs check
    task *taski = malloc(sizeof(task));
    taski->time = malloc(sizeof(struct timing));
    int r = timing_from_strings(taski->time, minutes_str, hours_str, daysofweek_str);
    if (r < 0)
    {
      perror("error while writing the time");
      exit(1);
    }

    uint32_t num;
    for (int i = 0; i < argc; i++)
    {
      //find alternative for null
      if (!strncmp(argv[i], "-m", 2) || !strncmp(argv[i], "-d", 2) || !strncmp(argv[i], "-H", 2))
      {
        num += 2;
      }
      if (strncmp(argv[i], "-c", 2) == 0)
      {
        num = i;
      }
    }
    taski->command = malloc(sizeof(commandline));
    taski->command->argc = argc - num - 1;
    taski->command->argv = malloc((argc - num - 1) * sizeof(str));
    for (uint32_t i = 0; i < argc - num - 1; i++)
    {
      taski->command->argv[i] = malloc(sizeof(str));
    }
    parse_argv(taski, num, argv);
    write_task(fd, taski);
    free(taski);
  }
  close(fd);
  // Add reading from reply pipe and printing in stdout
  mypipe = my_strcat((char *)PIPES_DIR, "/saturnd-reply-pipe");
  if (pipes_directory != NULL)
  {
    mypipe = my_strcat(pipes_directory, "/saturnd-reply-pipe");
  }
  fd = open(mypipe, O_RDWR | O_NONBLOCK);
  // TODO add select to check reply pipe changement ism
  int check;
  fd_set fds;

  FD_ZERO(&fds); // Clear FD set for select
  FD_SET(fd, &fds);
  while (1)
  {
    check = select(fd + 1, &fds, NULL, NULL, NULL);
    if (check < 0)
    {
      printf("Select error\n");
      exit(1);
    }
    if (check > 0 && FD_ISSET(fd, &fds))
    {

      break;
    }
  }
  if (fd == -1)
  {
    perror("error while openning request pipe");
    exit(1);
  }

  if (operation == CLIENT_REQUEST_LIST_TASKS)
  {
    uint16_t rep;
    uint32_t tasks;
    uint64_t op_id;
    uint64_t minutes;
    uint32_t hours;
    uint8_t daysofweek;
    int r = read(fd, &rep, 2);
    r = read(fd, &tasks, 4);
    SwapBytes(&tasks, 4);
    for (uint32_t i = 0; i < tasks; i++)
    {
      commandline *cmd = malloc(sizeof(commandline));
      r = read(fd, &op_id, 8);
      SwapBytes(&op_id, 8);
      r = read(fd, &minutes, sizeof(uint64_t));
      r = read(fd, &hours, sizeof(uint32_t));
      r = read(fd, &daysofweek, sizeof(uint8_t));
      printf("%llu: ", (unsigned long long)op_id);
      SwapBytes(&minutes, 8);
      SwapBytes(&hours, 4);
      struct timing *time = malloc(sizeof(struct timing));
      char *times = malloc(1024);
      time->minutes = minutes;
      time->hours = hours;
      time->daysofweek = daysofweek;
      timing_string_from_timing(times, time);
      free(time);
      printf("%s ", times);

      r = read(fd, &cmd->argc, 4);
      SwapBytes(&cmd->argc, 4);
      unsigned long num = (unsigned long)cmd->argc;
      cmd->argv = malloc(num * sizeof(str));
      for (unsigned long j = 0; j < num; j++)
      {
        cmd->argv[j] = malloc(sizeof(str));
        r = read(fd, &(cmd->argv[j]->len), 4);
        SwapBytes(&(cmd->argv[j]->len), 4);
        cmd->argv[j]->text = malloc((unsigned long)cmd->argv[j]->len);
        r = read(fd, cmd->argv[j]->text, cmd->argv[j]->len);
        if (j == num - 1)
          printf("%s\n", cmd->argv[j]->text);
        else
          printf("%s ", cmd->argv[j]->text);
        free(cmd->argv[j]);
      }
      free(cmd->argv);
      free(cmd);
    }
  }
  if (operation == CLIENT_REQUEST_CREATE_TASK)
  {
    uint16_t rep;
    uint64_t op_id;
    int r = read(fd, &rep, 2);
    r = read(fd, &op_id, 8);
    SwapBytes(&op_id, 8);
    printf("%llu\n", (unsigned long long)op_id);
  }

  if (operation == CLIENT_REQUEST_GET_TIMES_AND_EXITCODES)
  {
    uint16_t rep;
    uint32_t nbruns;
    int r = read(fd, &rep, 2);
    SwapBytes(&rep, 2);
    if (rep == SERVER_REPLY_OK)
    {
      
      r = read(fd, &nbruns, 4);
      //printf("NBRUNS IN CASSINI : %d\n", nbruns);
      SwapBytes(&nbruns, 4);
      int64_t time;
      uint16_t ex_code;
      //printf("NBRUNS IN CASSINI : %d\n", nbruns);
      for (size_t i = 0; i < nbruns; i++)
      {
        r = read(fd, &time, 8);
        SwapBytes(&time, 8);
        time_t rawtime = (time_t)time;
        struct tm *info = localtime(&rawtime);
        printf("%d-%02d-%02d %02d:%02d:%02d ", info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
        r = read(fd, &ex_code, 2);
        SwapBytes(&ex_code, 2);
        printf("%d\n", ex_code);
      }
    }
    else
    {
      // ER
      uint16_t err_code;
      r = read(fd, &err_code, 2);
      printf("%s\n", (char *)&err_code);
      exit(EXIT_FAILURE);
    }
  }

  if ((operation == CLIENT_REQUEST_GET_STDOUT) || (operation == CLIENT_REQUEST_GET_STDERR))
  {

    uint16_t rep;
    uint32_t len;
    int r = read(fd, &rep, 2);
    SwapBytes(&rep,2);
    if (rep == SERVER_REPLY_OK)
    {
      r = read(fd, &len, 4);

      SwapBytes(&len, 4);

      char *output = malloc(len);
      r = read(fd, output, len);
      printf("%s\n", output);
      free(output);
    }
    else
    {
      // ER
      uint16_t err_code;
      r = read(fd, &err_code, 2);
      printf("%s\n", (char *)&err_code);
      exit(EXIT_FAILURE);
    }
  }

  if (operation == CLIENT_REQUEST_REMOVE_TASK)
  {
    uint16_t rep;
    int r = read(fd, &rep, 2);
    SwapBytes(&rep, 2);
    if (rep == SERVER_REPLY_ERROR)
    {
      // ER
      uint16_t err_code;
      r = read(fd, &err_code, 2);
      printf("%s\n", (char *)&err_code);
      exit(EXIT_FAILURE);
    }
  }

  close(fd);
  free(mypipe);
  return EXIT_SUCCESS;

error:
  if (errno != 0)
    perror("main");
  free(pipes_directory);
  pipes_directory = NULL;
  return EXIT_FAILURE;
}
