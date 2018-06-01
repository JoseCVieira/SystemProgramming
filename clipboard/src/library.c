#include "clipboard_shared.h"
#include "clipboard.h"

int clipboard_connect(char * clipboard_dir){
    struct sockaddr_un server_addr;
    struct sockaddr_un client_addr;
    int sock_fd;

    /* Create socket */
    if((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        return sock_fd;

    /* Create client socket address => sun_family field always contains AF_UNIX | path to the socket */
    client_addr.sun_family = AF_UNIX;
    sprintf(client_addr.sun_path, clipboard_dir);

    /* Create server socket address => sun_family field always contains AF_UNIX | path to the socket */
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, "./CLIPBOARD_SOCKET");

    /* Connect to clipboard (server) */
    if(connect(sock_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
        return -1;
    
    return sock_fd;
}

int clipboard_copy(int clipboard_id, int region, void *buf, size_t count){
    message_t m;
    int result;
    char flag;
    
    if(count < 0 || count > MAX_SIZE)
        return 0;
    else if (!count) // dont let copy "nothing" to clipboard like computer's clipboard
        return 1;
    
    /* pass values to struct */
    m.operation = COPY;
    m.region = region;
    m.size = count;
    
    if(m.region >= 0 && m.region < NREGIONS){
        char* message;
        if ((message = (char*) malloc(sizeof(message_t))) == NULL){
            free(message);
            return 0;
        }
        
        /* serialize struct into char array */
        memcpy(message, &m, sizeof(message_t));

        /* send message_t to socket */
        if(write(clipboard_id, message, sizeof(message_t)) <= 0){
            free(message);
            return 0;
        }
        free(message);   
        
        if((result = read(clipboard_id, &flag, 1)) <= 0)
            return 0;
        
        if(!flag) 
            return 0;
        
        /* send buf to socket */
        if((result = write(clipboard_id, buf, count)) <= 0)
            return 0;
        return result;
    }else
        return 0;
}

int clipboard_paste(int clipboard_id, int region, void *buf, size_t count){
    message_t m;
    int result;
    char flag;
    
    if(count < 0 || count > MAX_SIZE)
        return 0;
    else if (!count) // dont let paste "nothing" like computer's clipboard
        return 1;
    
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
        if(write(clipboard_id, message, sizeof(message_t)) <= 0){
            free(message);
            return 0;
        }

        free(message);
        
        if((result = read(clipboard_id, &flag, 1)) <= 0)
            return 0;
        
        if(!flag) 
            return 0;
        
        /* read response from clipboard */
        if((result = read(clipboard_id, buf, count)) <= 0)
            return 0;
        return result;
        
    }else
        return 0;
}

int clipboard_wait(int clipboard_id, int region, void *buf, size_t count){
    message_t m;
    int result;
    
    // dont let wait for "nothing"
    if(count <= 0 || count > MAX_SIZE)
        return 0;
    
    /* pass values to struct */
    m.operation = WAIT;
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
        if(write(clipboard_id, message, sizeof(message_t)) <= 0){
            free(message);
            return 0;
        }

        free(message);
        
        /* read response from clipboard */
        if((result = read(clipboard_id, buf, count)) <= 0)
            return 0;
        return result;
        
    }else
        return 0;
}

void clipboard_close(int clipboard_id){
    close(clipboard_id);
}
