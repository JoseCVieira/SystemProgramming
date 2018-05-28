
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
#define NTHREADS 7
#define NREGIONS 10
#define MSGSIZE 100
void print_with_time(char * user_msg);
void test_string(char * user_msg,int id, int i);
void * thread_handler(void * args);
pthread_mutex_t id_mutex;
//cond. variable
pthread_cond_t data_cond = PTHREAD_COND_INITIALIZER;
int region = NREGIONS;
int main(int argv, char *argc[]){
    int  i,retval;
    if(argv  == 2)
	   region = atoi(argc[1]);
    pthread_t thread_id;
    for(i = 0; i < NTHREADS; i++){
      pthread_mutex_lock(&id_mutex);
      if(pthread_create(&thread_id, NULL, thread_handler, &i) != 0)
          printf("erro");
      pthread_mutex_lock(&id_mutex);
      pthread_mutex_unlock(&id_mutex);
    }
    pthread_join(thread_id,(void *) &retval);
    return 0;
}
void *thread_handler(void * args){
  int  i;
  char *buf;
  int id = *((int*)args);
  pthread_mutex_unlock(&id_mutex);
  if ((buf = (char*) malloc(  MSGSIZE * sizeof(char) )) == NULL){
      perror("[error] malloc");
      exit(-1);
  }

  printf("\n");
  /* Connect to clipboard (server) */
 sprintf(buf, "./socket_%d", getpid());
  int sock_fd = clipboard_connect(buf);

  if(sock_fd == -1){
      perror("[error] connect");
      exit(-1);
  }

  if(region == NREGIONS){
	  for(i = 0; i < NREGIONS; i++){
	    test_string(buf,id, i);
	      clipboard_copy(sock_fd, i,buf,MSGSIZE );
	  }
  }else{
  	test_string(buf, id, region);
  	clipboard_copy(sock_fd, region, buf, MSGSIZE);
  }
  free(buf);
  return 0;
}

void print_with_time(char * user_msg){
    struct tm *tm_struct;
    time_t time_v = time(NULL);
    tm_struct = localtime(&time_v);
    printf("<%02d:%02d:%02d> %s\n", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec, user_msg);
}
void test_string(char * user_msg, int id, int i){
    struct tm *tm_struct;
    if (realloc(user_msg, 100*sizeof(char) ) == NULL){
        perror("[error] malloc");
        exit(-1);
    }
    time_t time_v = time(NULL);
    tm_struct = localtime(&time_v);
    sprintf(user_msg, "<%02d:%02d:%02d> thread with ID: %d wrote on region %d\n",tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec, id, i);
}
