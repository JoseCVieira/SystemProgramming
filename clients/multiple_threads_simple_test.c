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
#include "app_test.c"
#include "../library/clipboard.h"

#define NTHREADS 1
#define NREGIONS 10
#define MSGSIZE 100

/*TEST ROUTINES*/
#define CPY_ONLY 0
#define CPY_ONLY_STR "-c"
#define CPY_PASTE 1
#define CPY_PASTE_STR "-cp"
#define TOTAL_OPT 3

int verifyInputArgs(int argc, char *argv[]);
void test_string(char * user_msg,int id, int i);
/*thread handlers*/
void *thread_cpy_paste_h(void * args);
void *thread_cpy_h(void * args);
/*optional test functions*/
void test_cpy(void);
void test_cpy_paste(void);
void app_test(void);



pthread_cond_t data_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_nr = PTHREAD_MUTEX_INITIALIZER;

int nr = 0;
int def_region = NREGIONS;
int nthreads_val = NTHREADS;
void (*test_functions[TOTAL_OPT]) = {test_cpy, test_cpy_paste, app_test};

int main(int argc, char *argv[]){
    pthread_t thread_id;
    int  *retval = NULL, test_routine;
    test_routine = verifyInputArgs(argc, argv);
    pthread_create(&thread_id, NULL, test_functions[test_routine], &test_routine);

    pthread_join(thread_id, (void *)retval);
    return 0;
}

/*thread handler copy case only*/
void * thread_cpy_h(void * args){
  int i, id = *((int*)args);
  pthread_mutex_unlock(&mutex);

  char *buf;

  if ((buf = (char*) malloc(  MSGSIZE * sizeof(char) )) == NULL){
      perror("[error] malloc");
      exit(-1);
  }

  int sock_fd = clipboard_connect(buf);
  sprintf(buf, "./socket_%d", getpid());

  if(sock_fd == -1){
      perror("[error] connect");
      exit(-1);
  }

  if(def_region >= NREGIONS || def_region < 0){
    for(i = 0; i < NREGIONS; i++){
        test_string(buf,id, i);
        clipboard_copy(sock_fd, i, buf, MSGSIZE);
    }
  }else{
    test_string(buf,id, def_region);
    clipboard_copy(sock_fd, def_region, buf, MSGSIZE);
  }

  clipboard_close(sock_fd);

  free(buf);

  pthread_mutex_lock(&mutex);
  nr++;
  pthread_mutex_unlock(&mutex);

  pthread_exit(NULL);
}

/*thread handler copy and paste case only*/

void * thread_cpy_paste_h(void * args){
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


    if(def_region >= NREGIONS || def_region < 0){
      for(i = 0; i < nthreads_val; i++){
          test_string(buf,id, i);
          clipboard_copy(sock_fd, i, buf, MSGSIZE);
      }

      for(i = 0; i < nthreads_val; i++){
          clipboard_paste(sock_fd, i, buf, MSGSIZE);
          printf("%s\n", buf);
      }
    }else{
      test_string(buf,id, def_region);
      clipboard_copy(sock_fd, def_region, buf, MSGSIZE);
      clipboard_paste(sock_fd, def_region, buf, MSGSIZE);
      printf("%s\n", buf);
    }

    clipboard_close(sock_fd);

    free(buf);

    pthread_mutex_lock(&mutex);
    nr++;
    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}

/*calling test function cpy*/
void test_cpy(){
  int i;
  pthread_t thread_id;
  for(i = 0; i < nthreads_val; i++){
      pthread_mutex_lock(&mutex);

      if(pthread_create(&thread_id, NULL, thread_cpy_h, &i) != 0)
          printf("erro");

      pthread_mutex_lock(&mutex);
      pthread_mutex_unlock(&mutex);
  }

  while(nr != nthreads_val);
}

/*calling test function cpy and paste*/
void test_cpy_paste(){
  int i;
  pthread_t thread_id;
  for(i = 0; i < nthreads_val; i++){
      pthread_mutex_lock(&mutex);

      if(pthread_create(&thread_id, NULL, thread_cpy_paste_h, &i) != 0)
          printf("erro");

      pthread_mutex_lock(&mutex);
      pthread_mutex_unlock(&mutex);
  }

  while(nr != nthreads_val);
}

/*Input verification*/
int verifyInputArgs(int argc, char *argv[]){
  int i;
  if(argc > 1){
    for(i =0; i < argc; i++){
      if(strcmp(argv[i],CPY_ONLY_STR) == 0){
        if(argv[i+1] != NULL)
          nthreads_val = atoi(argv[i+1]);
        if(argv[i+2] != NULL)
            def_region = atoi(argv[i+2]);
        return CPY_ONLY;
      }else if(strcmp(argv[i], CPY_PASTE_STR) == 0){
        if(argv[i+1] != NULL)
          nthreads_val = atoi(argv[i+1]);
        if(argv[i+2] != NULL)
          def_region = atoi(argv[i+2]);
        return CPY_PASTE;
      }
    }
  }
  return TOTAL_OPT-1;

}
void test_string(char * user_msg, int id, int i){
    struct tm *tm_struct;
    struct timeval tv;

    if (realloc(user_msg, 100*sizeof(char) ) == NULL){
        perror("[error] malloc");
        exit(-1);
    }
    gettimeofday(&tv, 0);
    time_t time_v = time(NULL);
    tm_struct = localtime(&time_v);
    sprintf(user_msg, "<%02d:%02d:%02d:%04lu> thread with ID: %d wrote on region %d\n",tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec,tv.tv_usec, id, i);
}
