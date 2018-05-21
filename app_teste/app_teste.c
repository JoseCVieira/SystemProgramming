#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include "../clipboard/src/clipboard.h"

#define COPY 1
#define PASTE 2
#define WAIT 3
#define CLOSE 4

struct tm *tm_struct;

void print_with_time(char * user_msg);

int main(){
    int region, operation;
    char *buf = NULL, socket_name[20];
    size_t n = 0;
    ssize_t nchr = 0;

    /* Menu */
    printf("\n\n");
    printf("+------------------------------+\n");
    printf("|             MENU             |\n");
    printf("+------------------------------+\n");
    printf("| copy  - 1 <region> <message> |\n");
    printf("| paste - 2 <region> <length>  |\n"); //fazer isto
    printf("| wait  - 3 <region> <length>  |\n");
    printf("| close - 4                    |\n");
    printf("+------------------------------+\n");
    
    /* Connect to clipboard (server) */
    sprintf(socket_name, "./socket_%d", getpid());
    int sock_fd = clipboard_connect(socket_name);
    if(sock_fd == -1){
        perror("[error] connect");
        exit(-1);
    }

    while(1){
        printf("\noption: ");
        
        n = 0;
        nchr = 0;

        if(buf != NULL && strlen(buf))
            memset(buf, 0, strlen(buf));
        
        if ((nchr = getline (&buf, &n, stdin)) != -1)
            buf[--nchr] = 0;  // strip newline
        
        if(isdigit(buf[0]) && isdigit(buf[2])){
            
            /* pass values to struct clipboard */
            operation = buf[0] - '0';
            region = buf[2] - '0';
            
            memmove(buf, buf+4, (nchr+4) + 4);

            if(operation == COPY){
<<<<<<< HEAD
                if(!clipboard_copy(sock_fd, 11, buf, sizeof m.message))
=======
                if(!clipboard_copy(sock_fd, region, buf, strlen(buf)))
>>>>>>> 295e5be8a13ff4c26faa1186b6ee34ee613bb50c
                    print_with_time("communication error.");
                else
                    print_with_time("OK");
            }else if(operation == PASTE){
                if(!clipboard_paste(sock_fd, region, buf, 100*sizeof(char))) //mudar isto!
                    print_with_time("communication error.");
                else
                    print_with_time(buf);
                
            }else if(operation == WAIT){
                print_with_time("waiting...\n");
                if(!clipboard_wait(sock_fd, region, buf, 100*sizeof(char))) //mudar isto!
                    print_with_time("communication error.");
                else
                    print_with_time(buf);
            }else
                print_with_time("invalid option.");
           
        }else if(isdigit(buf[0])){
            operation = buf[0] - '0';
            if(operation == CLOSE){
                clipboard_close(sock_fd);
                print_with_time("close socket and exiting\n");
                exit(0);
            }else
                print_with_time("invalid option."); 
        }else
            print_with_time("invalid option.");
    }
}

void print_with_time(char * user_msg){
    time_t time_v = time(NULL);
    tm_struct = localtime(&time_v);
    printf("<%02d:%02d:%02d> %s\n", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec, user_msg);
}
