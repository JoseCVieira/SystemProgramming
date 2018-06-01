#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <errno.h>
#include <time.h>

#include "clipboard_shared.h"

#define SOCK_ADDRESS "./CLIPBOARD_SOCKET"

#define CLOSE 4

#define NR_BACKLOG 10
#define INITIAL_NR_FD 10
#define MAX_NR_FD 1015

#define ADD_FD 1
#define RMV_FD -1

#define MIN_PORT 1024
#define MAX_PORT 64738

#define LOCAL 1
#define REMOTE_C 2
#define REMOTE_S 3
#define REMOTE_T 4

#define TMSTAMP 4

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
#define E_T_DETACH "[error] thread detach"
#define E_SIG      "[error] sigaction"

#define I_INPUT_USAGE "[invalid] usage: ./clipboard < -c ip port >\n"
#define I_OPTION      "[invalid] option\n"
#define I_IP          "[invalid] ip address\n"
#define I_PORT        "[invalid] port\n"

#define NR_MUTEXES 8
#define M_USER     0
#define M_THREAD   1
#define M_CPY_L    2
#define M_CPY_R    3
#define M_REPL     4
#define M_DATA_C   5
#define M_REPLI    6
#define M_WAIT_FD  7

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

typedef struct s_timestamp_msg {
    uint32_t usec;
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint32_t year;
    uint16_t yday;
    uint32_t isdst;
    uint8_t wday;
}timestamp_msg_t;

typedef struct s_timestamp {
    struct timeval tv;
    struct tm tm_struct;
}timestamp_t;

typedef struct s_clipboard {
    void *data;
    size_t size;
    timestamp_t ts;
}clipboard_t;

// functions prototypes
client_t connect_server(char *ipAddress, int port, int sock_fd);
int compare_timestamp(timestamp_t ts1, timestamp_t ts2);
int update_client_fds(client_t client, int operation);
timestamp_t get_timestamp_ntoh(timestamp_msg_t msg);
timestamp_msg_t get_timestamp_hton(timestamp_t ts);
void verifyInputArguments(int argc, char *argv[]);
void print_timestamp(timestamp_t ts ,char * msg);
int isValidIpAddress(char *ipAddress);
void open_timestamp_socket();
timestamp_t get_timestamp();
void secure_exit(int flag);
void open_remote_socket();
void open_local_socket();
void p_error(char* msg);
void init_time_clip();
void inv(char* msg);
void init();

// thread handlers prototypes
void* accept_timestamp_client_handler(void *args);
void* accept_remote_client_handler(void *args);
void* accept_local_client_handler(void *args);
void* timestamp_thread_handler(void *args);
void* remote_thread_handler(void *args);
void* local_thread_handler(void *args);
void* replicate_copy_cmd(void *args);

// sigaction handlers prototypes
void ctrl_c_callback_handler();
void broken_pipe_callback_handler();
