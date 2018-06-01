#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define NREGIONS 10
#define COPY 1
#define PASTE 2
#define WAIT 3

#define MAX_SIZE 1073741824 //1GB

typedef struct s_message {
    uint8_t operation;
    uint8_t region;
    uint64_t size;
}message_t;
