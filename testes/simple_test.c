#include "../clipboard/src/clipboard.h"

void print_with_time(char * user_msg);
void test_string(char * user_msg, int i);

int main(){
    int  i;
    message_t m;
    char *buf;

    if ((buf = (char*) malloc( sizeof m.message + 4*sizeof(char) )) == NULL){
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

    for(i = 0; i < NREGIONS; i++){
      test_string(buf, i);
      clipboard_copy(sock_fd, i,buf,100 );
    }

    for(i = 0; i < NREGIONS; i++){
      test_string(buf, i);
      clipboard_paste(sock_fd, i,buf,100 );
      printf("%s\n", buf);
    }

    return 0;
}

void print_with_time(char * user_msg){
    time_t time_v = time(NULL);
    tm_struct = localtime(&time_v);
    printf("<%02d:%02d:%02d> %s\n", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec, user_msg);
}
void test_string(char * user_msg, int i){
    time_t time_v = time(NULL);
    tm_struct = localtime(&time_v);
    sprintf(user_msg, "<%02d:%02d:%02d> process with pid: %d wrote on region %d\n",tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec, getpid(), i);
}
