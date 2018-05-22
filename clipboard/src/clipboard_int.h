#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
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
#include <errno.h>

#define SOCK_ADDRESS "./CLIPBOARD_SOCKET"

#define MSG_SIZE 100
#define NREGIONS 10

#define COPY 1
#define PASTE 2
#define WAIT 3
#define CLOSE 4

#define NR_BACKLOG 10
#define INITIAL_NR_FD 10

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
#define E_TIME     "[error] time to wait"

#define I_INPUT_USAGE "[invalid] usage: ./clipboard < -c ip port >\n"
#define I_OPTION      "[invalid] option\n"
#define I_IP          "[invalid] ip address\n"
#define I_PORT        "[invalid] port\n"

typedef struct s_message {
    int operation;
    int region;
    size_t size;
}message_t;

typedef struct s_clipboard {
    void *data;
    size_t size;
}clipboard_t;

typedef struct s_client {
    int fd;
    int type;
    int wait;
    size_t wait_size;
    char sin_addr[20];
}client_t;

typedef struct s_replicate {
    client_t client;
    message_t message;
    void *data;
}replicate_t;

// functions prototypes
void update_client_fds(client_t client, int operation);
void verifyInputArguments(int argc, char *argv[]);
client_t connect_server(char *ipAddress, int port);
int isValidIpAddress(char *ipAddress);
void secure_exit(int status);
void open_remote_socket();
void open_local_socket();
void p_error(char* msg);
void inv(char* msg);
void init();

// thread handlers prototypes
void* accept_remote_client_handler(void *args);
void* accept_local_client_handler(void *args);
void* remote_thread_handler(void *args);
void* local_thread_handler(void *args);
void* replicate_copy_cmd(void *args);

// sigaction handlers prototypes
void ctrl_c_callback_handler(int signum);
