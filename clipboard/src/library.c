#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "clipboard.h"

#define SOCK_ADDRESS "/tmp/CLIPBOARD_SOCKET"
#define COPY 1
#define PASTE 2
#define WAIT 3
#define NREGIONS 10

typedef struct s_message {
    int operation;
    int region;
    size_t size;
}message_t;

int clipboard_connect(char * clipboard_dir){
    struct sockaddr_un server_addr;
    struct sockaddr_un client_addr;

    /* Create socket */
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sock_fd == -1) return sock_fd;

    /* Create client socket address => sun_family field always contains AF_UNIX | path to the socket */
    client_addr.sun_family = AF_UNIX;
    sprintf(client_addr.sun_path, clipboard_dir);

    /* Create server socket address => sun_family field always contains AF_UNIX | path to the socket */
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, SOCK_ADDRESS);

    /* Connect to clipboard (server) */
    if(connect(sock_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
        return -1;
    
    return sock_fd;
}

int clipboard_copy(int clipboard_id, int region, void *buf, size_t count){
    message_t m;
    int result;

    /* pass values to struct */
    m.operation = COPY;
    m.region = region;
    m.size = count;
    
    if(m.region >= 0 && m.region < NREGIONS){
        if((int)count){
            char* message;
            if ((message = (char*) malloc(sizeof(message_t))) == NULL){
                free(message);
                return 0;
            }
            
            /* serialize struct into char array */
            memcpy(message, &m, sizeof(message_t));

            /* send message_t to socket */
            if(write(clipboard_id, message, sizeof(message_t)) == -1){
                free(message);
                return 0;
            }
            
            free(message);     
            
            /* send buf to socket */
            if((result = write(clipboard_id, buf, count)) == -1)
                return 0;
            return result;
        }else
            return 1; // dont let copy "nothing" to clipboard like computer's clipboard
        
    }else
        return 0;
}

int clipboard_paste(int clipboard_id, int region, void *buf, size_t count){
    message_t m;
    int result;
    
    /* pass values to struct */
    m.operation = PASTE;
    m.region = region;
    m.size = count;

    if(m.region >= 0 && m.region < NREGIONS){
        char* message;
        if ((message = (char*) malloc( sizeof(message_t) )) == NULL){
            free(message);
            return 0;
        }

        /* serialize struct into char array */
        memcpy(message, &m, sizeof(message_t));

        /* send message_t to socket */
        if(write(clipboard_id, message, sizeof(message_t)) == -1){
            free(message);
            return 0;
        }

        free(message);
        
        /* read response from clipboard */
        if((result = read(clipboard_id, buf, count)) == -1)
            return 0;
        return result;
        
    }else
        return 0;
}

int clipboard_wait(int clipboard_id, int region, void *buf, size_t count){
    return 0;
}

void clipboard_close(int clipboard_id){
    close(clipboard_id);
}
