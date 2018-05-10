#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <time.h>

#define SOCK_ADDRESS "/tmp/CLIPBOARD_SOCKET"

#define MSG_SIZE 100
#define NREGIONS 10

#define COPY 1
#define PASTE 2
#define CLOSE 3

#define NR_BACKLOG 10
#define INITIAL_NR_FD 1

#define ADD_FD 1
#define RMV_FD -1

#define MIN_PORT 1024
#define MAX_PORT 64738

#define LOCAL 1
#define REMOTE_C 2
#define REMOTE_S 3

// messages
#define E_SOCKET   "[error] socket"
#define E_BIND     "[error] bind"
#define E_LISTEN   "[error] listen"
#define E_CONN     "[error] connect"
#define E_MALLOC   "[error] malloc"
#define E_REALLOC  "[error] realloc"
#define E_ACCEPT   "[error] accept"
#define E_MUTEX    "[error] mutex"
#define E_RWLOCK   "[error] rwlock"
#define E_RECV     "[error] recv"
#define E_WRITE    "[error] write"
#define E_T_CREATE "[error] thread create"
#define E_SET_FLAG "[error] set flag"

#define I_INPUT_USAGE "[invalid] usage: ./clipboard < -c ip port >\n"
#define I_OPTION      "[invalid] option\n"
#define I_IP          "[invalid] ip address\n"
#define I_PORT        "[invalid] port\n"

typedef struct s_message {
    int operation;
    int region;
    char message[MSG_SIZE];
}message_t;

typedef struct s_client {
    int fd;
    int type;
    char sin_addr[20];
}client_t;

typedef struct s_replicate {
    client_t client;
    char message[sizeof(message_t)];
}replicate_t;

struct tm *tm_struct;

int clipboard_connect(char * clipboard_dir);
int clipboard_copy(int clipboard_id, int region, void *buf, size_t count);
int clipboard_paste(int clipboard_id, int region, void *buf, size_t count);
//int clipboard_wait(int clipboard_id, int region, void *buf, size_t count);
void clipboard_close(int clipboard_id);
