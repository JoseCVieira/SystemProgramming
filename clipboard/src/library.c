#include "clipboard.h"

int clipboard_connect(char * clipboard_dir){
    struct sockaddr_un server_addr;
    struct sockaddr_un client_addr;

    /* Create socket */
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd == -1){
        perror("[error] socket");
        exit(-1);
    }

    /* Create client socket address => sun_family field always contains AF_UNIX | path to the socket */
    client_addr.sun_family = AF_UNIX;
    sprintf(client_addr.sun_path, clipboard_dir);

    /* Create server socket address => sun_family field always contains AF_UNIX | path to the socket */
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, SOCK_ADDRESS);

    /* Connect to clipboard (server) */
    int err_c = connect(sock_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr));
    if(err_c==-1){
        perror("[error] connect");
        exit(-1);
    }
    return sock_fd;
}

int clipboard_copy(int clipboard_id, int region, void *buf, size_t count){ //ver esta frunção
    message_t m;
    int result;
    char* message;

    if ((message = (char*) malloc(sizeof(message_t))) == NULL){
        perror("[error] malloc");
        exit(-1);
    }

    /* pass values to struct */
    m.operation = COPY;
    m.region = region;
    
    if(m.region >= 0 && m.region < NREGIONS){
        memcpy(m.message, buf, count);

        /* serialize struct into message */
        memcpy(message, &m, sizeof(m));

        /* send message to socket */
        if((result = write(clipboard_id, message, sizeof(m))) == -1)
            result=0;
        
    }else
        result = 0;
    
    free(message);
    return result;
}

int clipboard_paste(int clipboard_id, int region, void *buf, size_t count){
    message_t m;
    int result;
    char* message;

    if ((message = (char*) malloc( sizeof(message_t) )) == NULL){
        perror("[error] malloc");
        exit(-1);
    }

    /* pass values to struct */
    m.operation = PASTE;
    m.region = region;

    if(m.region >= 0 && m.region < NREGIONS){
        /* serialize struct into message */
        memcpy(message, &m, sizeof(message_t));

        /* send message to socket */
        if((result = write(clipboard_id, message, sizeof (message_t))) == -1){
            free(message);
            return (result=0);
        }

        /* read response from clipboard */
        if((result = read(clipboard_id, message,  sizeof(message_t))) == -1){
            free(message);
            return (result=0);
        }

        /* message to message_t */
        memcpy(&m, message, sizeof(message_t));

        /* get message to print */
        memcpy(buf, m.message, count);
    }else
        result = 0;
    
    free(message);
    return result;
}

/*int clipboard_wait(int clipboard_id, int region, void *buf, size_t count){
    
}*/

void clipboard_close(int clipboard_id){
    close(clipboard_id);
}
