#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <linux/limits.h>
#include "common.h"
#include "tasks.h"
#include <wait.h>

// Fichier contenant les fonctions communes au dameon et au client

void SwapBytes(void *pv, size_t n)
{

  char *p = pv;
  size_t lo, hi;
  for (lo = 0, hi = n - 1; hi > lo; lo++, hi--)
  {
    char tmp = p[lo];
    p[lo] = p[hi];
    p[hi] = tmp;
  }
}

char *reverse_uint16(uint16_t operation)
{

  char *c16;
  c16 = malloc(2);
  for (int i = 1; i >= 0; i--)
  {
    c16[i] = operation & 0xFF;
    operation >>= 8;
  }
  return c16;
}

char *reverse_uint32(uint32_t operation)
{

  char *c32;
  c32 = malloc(4);
  for (int i = 3; i >= 0; i--)
  {
    c32[i] = operation & 0xFF;
    operation >>= 8;
  }

  return c32;
}

char *reverse_uint64(uint64_t operation)
{

  char *c64;
  c64 = malloc(8);
  for (int i = 7; i >= 0; i--)
  {
    c64[i] = operation & 0xFF;
    operation >>= 8;
  }
  return c64;
}

char *uint16(uint16_t operation)
{

  char *c16;
  c16 = malloc(2);
  for (int i = 0; i < 2; i++)
  {
    c16[i] = operation & 0xFF;
    operation >>= 8;
  }
  return c16;
}

char *uint32(uint32_t operation)
{

  char *c32;
  c32 = malloc(4);
  for (int i = 0; i < 4; i++)
  {
    c32[i] = operation & 0xFF;
    operation >>= 8;
  }

  return c32;
}

char *uint64(uint64_t operation)
{

  char *c64;
  c64 = malloc(8);
  for (int i = 0; i < 8; i++)
  {
    c64[i] = operation & 0xFF;
    operation >>= 8;
  }
  return c64;
}

char *my_strcat(char *strg1, char *strg2)
{
  char *str3 = (char *)malloc(1 + sizeof(char *) * (strlen(strg1) + strlen(strg2)));
  strcpy(str3, strg1);
  strcat(str3, strg2);
  return str3;
}

char *concatenate(char *a, const char *b, char *c)
{
  int size = strlen(a) + strlen(b) + strlen(c) + 1;
  char *str = malloc(size);
  strcpy(str, a);
  strcat(str, b);
  strcat(str, c);

  return str;
}

int mkdir_p(const char *path)
{
  const size_t len = strlen(path);
  char _path[PATH_MAX];
  char *p;

  errno = 0;

  /* Copy string so its mutable */
  if (len > sizeof(_path) - 1)
  {
    errno = ENAMETOOLONG;
    return -1;
  }
  strcpy(_path, path);

  /* Iterate the string */
  for (p = _path + 1; *p; p++)
  {
    if (*p == '/')
    {
      /* Temporarily truncate */
      *p = '\0';

      if (mkdir(_path, S_IRWXU) != 0)
      {
        if (errno != EEXIST)
          return -1;
      }

      *p = '/';
    }
  }

  if (mkdir(_path, S_IRWXU) != 0)
  {
    if (errno != EEXIST)
      return -1;
  }

  return 0;
}

pid_t proc_find(const char *name)
{
  DIR *dir;
  struct dirent *ent;
  char *endptr;
  char buf[512];

  if (!(dir = opendir("/proc")))
  {
    perror("can't open /proc");
    return -1;
  }

  while ((ent = readdir(dir)) != NULL)
  {
    /* if endptr is not a null character, the directory is not
     * entirely numeric, so ignore it */
    long lpid = strtol(ent->d_name, &endptr, 10);
    if (*endptr != '\0')
    {
      continue;
    }
    snprintf(buf, sizeof(buf), "/proc/%ld/stat", lpid);
    /* try to open the cmdline file */
    FILE *fp = fopen(buf, "r");
    if (fp)
    {
      if (fgets(buf, sizeof(buf), fp) != NULL)
      {
        /* check the second token in the file, the program name */
        char *first = strtok(buf, " ");
        first = strtok(NULL, " ");
        if (!strcmp(first, name))
        {
          fclose(fp);
          closedir(dir);
          return (pid_t)lpid;
        }
      }
      fclose(fp);
    }
  }

  closedir(dir);
  return -1;
}

int make_daemon()
{
  if (proc_find("(saturnd)") == -1)
  {
    const char *args[] = {"./saturnd", NULL};
    int pid = fork();
    if (pid == 0)
    {
      int fd = open("/dev/null", O_WRONLY);
      dup2(fd, 0);
      dup2(fd, 1);
      dup2(fd, 2);
      close(fd);
      execlp("./saturnd", "./saturnd", NULL);
      exit(0);
    }
    else if (pid == -1)
      return -1;
    else
      waitpid(-1, NULL, 0);
    return 0;
  }
  return -1;
}

int write_task(int fd, task *t)
{
  uint64_t minutes = t->time->minutes;
  uint64_t hours = t->time->hours;
  SwapBytes(&minutes, 8);
  SwapBytes(&hours, 4);
  int w = write(fd, &minutes, 8);
  w = write(fd, &hours, 4);
  w = write(fd, &t->time->daysofweek, 1);
  int num = t->command->argc;
  SwapBytes(&num, 4);
  w = write(fd, &num, 4);
  for (uint32_t i = 0; i < t->command->argc; i++)
  {

    uint32_t sz = t->command->argv[i]->len;

    SwapBytes(&sz, 4);

    w = write(fd, &sz, 4);
    w = write(fd, t->command->argv[i]->text, t->command->argv[i]->len);
  }
  return 0;
}

void file_to_stdout(int fd_out, int fd_pipe)
{
  str *out = malloc(sizeof(str));
  int r = lseek(fd_out, 0, SEEK_END);
  out->len = r;
  SwapBytes(&r, 4);
  write(fd_pipe,&r,4);
  lseek(fd_out, 0, SEEK_SET);
  char *buf = malloc(out->len+1);
  read(fd_out,buf,out->len);
  buf[out->len] = NULL;
  write(fd_pipe,buf,strlen(buf));
  free(buf);
}
