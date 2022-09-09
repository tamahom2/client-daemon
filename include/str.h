#ifndef STR_H
#define STR_H

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>


#include "client-request.h"
#include "server-reply.h"

typedef struct
{
  uint32_t len;
  char *text;

} str;


#endif // STR
