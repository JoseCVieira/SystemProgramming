#include "../clipboard/src/clipboard.h"

void print_with_time(char * user_msg);

int main(){
    int region, operation;
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

    /* Menu */
    printf("\n");
    printf("+------------------------------+\n");
    printf("|             MENU             |\n");
    printf("+------------------------------+\n");
    printf("| copy  - 1 <region> <message> |\n");
    printf("| paste - 2 <region>           |\n");
    printf("| close - 3                    |\n");
    printf("+------------------------------+\n");

    while(1){
        printf("\noption: ");
        
        fgets(buf, MSG_SIZE, stdin);
        strtok(buf, "\n");        
        
        if(isdigit(buf[0]) && isdigit(buf[2])){
            
            /* pass values to struct clipboard */
            operation = buf[0] - '0';
            region = buf[2] - '0';
            
            memcpy(buf, buf + 4*sizeof(char), sizeof m.message);

            if(operation == COPY){
                if(!clipboard_copy(sock_fd, 11, buf, sizeof m.message))
                    print_with_time("communication error.");
                else
                    print_with_time("OK");
                
            }else if(operation == PASTE){
                if(!clipboard_paste(sock_fd, region, buf, sizeof m.message))
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
