#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>


#include "client-request.h"
#include "server-reply.h"
#include "str.h"

typedef struct
{
  uint32_t argc;
  str **argv;

} commandline;



#endif // COMMANDLINE
