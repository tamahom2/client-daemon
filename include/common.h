#ifndef COMMON_H_
#define COMMON_H_

#define PIPES_DIR "./run/pipes"

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "tasks.h"

void SwapBytes(void *pv, size_t n);
char *reverse_uint16(uint16_t operation);
char *reverse_uint32(uint32_t operation);
char *reverse_uint64(uint64_t operation);
char *uint16(uint16_t operation);
char *uint32(uint32_t operation);
char *uint64(uint64_t operation);
char *my_strcat(char *strg1, char *strg2);
char *concatenate(char *a, const char *b, char *c);
void rek_mkdir(char *path);
int write_task(int fd, task *t);
void file_to_stdout(int fd_out, int fd_pipe);

#endif