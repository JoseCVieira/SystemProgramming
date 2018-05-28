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

#include "../library/clipboard.h"

#define NTHREADS 1
#define NREGIONS 10
#define MSGSIZE 100

void print_with_time(char * user_msg);
void test_string(char * user_msg,int id, int i);
void * thread_handler(void * args);

pthread_cond_t data_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_nr = PTHREAD_MUTEX_INITIALIZER;

int nr = 0;

int main(){
    int i;
    pthread_t thread_id;

    for(i = 0; i < NTHREADS; i++){
        pthread_mutex_lock(&mutex);

        if(pthread_create(&thread_id, NULL, thread_handler, &i) != 0)
            printf("erro");

        pthread_mutex_lock(&mutex);
        pthread_mutex_unlock(&mutex);
    }

    while(nr != NTHREADS);

    return 0;
}

void * thread_handler(void * args){
    int i, id = *((int*)args);
    pthread_mutex_unlock(&mutex);

    char *buf;

    if ((buf = (char*) malloc(  MSGSIZE * sizeof(char) )) == NULL){
        perror("[error] malloc");
        exit(-1);
    }

    sprintf(buf, "./socket_%d", getpid());
    int sock_fd = clipboard_connect(buf);

    if(sock_fd == -1){
        perror("[error] connect");
        exit(-1);
    }

    for(i = 0; i < NREGIONS; i++){
        test_string(buf,id, i);
        clipboard_copy(sock_fd, i, buf, MSGSIZE);
    }

    for(i = 0; i < NREGIONS; i++){
        clipboard_paste(sock_fd, i, buf, MSGSIZE);
        printf("%s\n", buf);
    }

    clipboard_close(sock_fd);

    free(buf);

    pthread_mutex_lock(&mutex);
    nr++;
    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}

void print_with_time(char * user_msg){
    struct tm *tm_struct;
    time_t time_v = time(NULL);
    tm_struct = localtime(&time_v);
    printf("<%02d:%02d:%02d> %s\n", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec, user_msg);
}

void test_string(char * user_msg, int id, int i){
    struct tm *tm_struct;
    struct timeval tv;

    if (realloc(user_msg, 100*sizeof(char) ) == NULL){
        perror("[error] malloc");
        exit(-1);
    }
    gettimeofday(&tv, 0);
    printf("%lu:%lu\n",tv.tv_sec, tv.tv_usec);
    time_t time_v = time(NULL);
    tm_struct = localtime(&time_v);
    sprintf(user_msg, "<%02d:%02d:%02d:%04lu> thread with ID: %d wrote on region %d\n",tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec,tv.tv_usec, id, i);
}
