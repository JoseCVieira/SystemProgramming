#include "src/clipboard.h"
#define NAME "\n***CLIPBOARD***\n\n"

/* Function prototypes */
void update_client_fds(client_fd client, int operation);
void verifyInputArguments(int argc, char *argv[]);
void connect_server(char *ipAddress, int port);
int isValidIpAddress(char *ipAddress);
void secure_exit(int status);
void open_remote_socket();
void open_local_socket();
void p_error(char* msg);
void inv(char* msg);
void init();

// thread handlers prototypes
void* accept_remote_client_handler(void *args);
void* accept_local_client_handler(void *args);
void* remote_thread_handler(void *args);
void* local_thread_handler(void *args);

// Sigaction handlers prototypes
void ctrl_c_callback_handler(int signum);

/* Globals */
pthread_mutex_t mutex_fd       = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cpy_l_fd = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cpy_r_fd = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlock_t rwlock_clip[NREGIONS];

// vulnerable to race condition
message_t clipboard[NREGIONS];
client_fd *all_client_fd;
int nr_threads = 0;

// not vulnerable
int remote_connection = 0, l_sock_fd, r_out_sock_fd, r_in_sock_fd;

int main(int argc, char* argv[]){
    pthread_t thread_id;
    
    init();                                  // allocate memory and initialize mutexes
    verifyInputArguments(argc, argv);        // verify input arguments and create socket to remote backup if needded
    signal(SIGINT, ctrl_c_callback_handler); // SIGINT CTRL-C
    open_local_socket();                     // open, bind and listen local socket
    open_remote_socket();                    // open, bind and listen remote socket

    if(pthread_create(&thread_id, NULL, accept_local_client_handler, NULL) != 0)  // thread to handle accept local clients
        p_error(E_T_CREATE);
    if(pthread_create(&thread_id, NULL, accept_remote_client_handler, NULL) != 0) // thread to handle accept remote clients
        p_error(E_T_CREATE);
    
    while(nr_threads != 2);
    printf("[log] thread main: threadID:%lu\n\t- all set!\n\n", pthread_self());
    
    while(1){sleep(1);} //falta remover isto
    exit(0); // never happens
}

void* local_thread_handler(void* args){
    client_fd client;
    client.fd = *((int *) args);
    pthread_mutex_unlock(&mutex_cpy_l_fd);
    
    client.type = LOCAL;
    
    char message[sizeof(message_t)];
    message_t m1;
    
    pthread_mutex_lock(&mutex_fd);
    update_client_fds(client, ADD_FD);
    pthread_mutex_unlock(&mutex_fd);
    
    printf("[log] thread: threadID=%lu\n\t- was created to handle communication with the new local client\n\n", pthread_self());

    while(recv(client.fd, message, sizeof m1, 0) > 0) {
        memcpy(&m1, message, sizeof m1);
        
        printf("[log] thread: threadID=%lu\n", pthread_self());

        if(m1.operation == COPY) {
            /* overwrite region */
            pthread_rwlock_wrlock(&rwlock_clip[m1.region]);
            memcpy(&clipboard[m1.region], message, sizeof clipboard[m1.region]);
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);
            
            pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
            printf("\t- recv[l]: region=%d | message=%s\n", m1.region, clipboard[m1.region].message);
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);

            if(remote_connection) {
                /* send received data to the remote backup */
                if(write(r_out_sock_fd, message, sizeof clipboard[m1.region]) == -1)
                    p_error(E_WRITE);
                
                pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
                printf("\t- sent[r]: region=%d | message=%s\n", m1.region, clipboard[m1.region].message);
                pthread_rwlock_unlock(&rwlock_clip[m1.region]);
            }
            printf("\n");

        }else if(m1.operation == PASTE){
            /* copy struct to message */
            pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
            memcpy(message, &clipboard[m1.region], sizeof clipboard[m1.region]);
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);

            /* send message to socket */
            if(write(client.fd, message, sizeof clipboard[m1.region]) == -1)
                p_error(E_WRITE);

            pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
            printf("\t- sent[l]: region=%d | message=%s\n\n", m1.region, clipboard[m1.region].message);
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);
        }
    }
    printf("[log] thread: threadID=%lu\n\t- closing connection and exiting.\n\n", pthread_self());

    /* when it receives EOF closes connection */
    close(client.fd);
    
    pthread_mutex_lock(&mutex_fd);
    update_client_fds(client, RMV_FD);
    pthread_mutex_unlock(&mutex_fd);
    
    //will exit the thread that calls it.
    pthread_exit(NULL);
}

void* remote_thread_handler(void *args){
    remote remote_t = *((remote *) args);
    pthread_mutex_unlock(&mutex_cpy_r_fd);
    
    client_fd client;
    client.fd = remote_t.fd;
    client.type = REMOTE;

    pthread_mutex_lock(&mutex_fd);
    update_client_fds(client, ADD_FD);
    pthread_mutex_unlock(&mutex_fd);
    
    char message[sizeof(message_t)];
    int region;
    message_t m1;
    
    printf("[log] thread: threadID=%lu\n\t- was created to handle communication with the new remote client: %s\n\n", pthread_self(), remote_t.sin_addr);

    for(region = 0; region < NREGIONS; region++){
        pthread_rwlock_rdlock(&rwlock_clip[region]);
        memcpy(message, &clipboard[region], sizeof clipboard[region]); //copy struct to message
        pthread_rwlock_unlock(&rwlock_clip[region]);
        
        if(write(remote_t.fd, message, sizeof clipboard[region]) == -1) //send message to clipboard
            p_error(E_WRITE);
        
        pthread_rwlock_rdlock(&rwlock_clip[region]);
        printf("\t- sent[r]: region=%d | message=%s\n", region, clipboard[region].message);
        pthread_rwlock_unlock(&rwlock_clip[region]);
    }
    printf("\n");

    while(recv(remote_t.fd, message, sizeof(message_t), 0) > 0){
        memcpy(&m1, message,  sizeof m1);
        
        printf("[log] thread: threadID=%lu\n", pthread_self());

        if(m1.operation == COPY){
            /* overwrite region */
            pthread_rwlock_wrlock(&rwlock_clip[m1.region]);
            memcpy(&clipboard[m1.region], message, sizeof clipboard[m1.region]);
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);
            
            pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
            printf("\t- recv[r]: region=%d | message=%s\n\n", m1.region, clipboard[m1.region].message);
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);
        }

    }
    
    printf("[log] thread: threadID=%lu\n\t- closing connection and exiting.\n\n", pthread_self());

    /* when it receives EOF closes connection */
    close(remote_t.fd);
    
    pthread_mutex_lock(&mutex_fd);
    update_client_fds(client, RMV_FD);
    pthread_mutex_unlock(&mutex_fd);

    //will exit the thread that calls it.
    pthread_exit(NULL);
}

void* accept_local_client_handler(void *args){
    struct sockaddr_un client_addr;
    socklen_t size_addr;
    pthread_t thread_id;
    int client_fd;
    
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sun_family = AF_UNIX;
    strcpy(client_addr.sun_path, SOCK_ADDRESS);
    
    printf("\n[log] thread: threadID=%lu\n\t- was created to handle accept local clients\n", pthread_self());
    
    pthread_mutex_lock(&mutex_fd);
    nr_threads++;
    pthread_mutex_unlock(&mutex_fd);

    /* Accept new connection from a new local client */
    while((client_fd = accept(l_sock_fd, (struct sockaddr *) &client_addr, &size_addr)) != -1){

        if(client_fd == -1)
            p_error(E_ACCEPT);
        
        printf("[log] thread: threadID=%lu\n\t- acceped connection from: %s\n\n", pthread_self(), client_addr.sun_path);
        
        pthread_mutex_lock(&mutex_cpy_l_fd);
        
        // create a new thread to handle the communication with the new local client
        if(pthread_create(&thread_id, NULL, local_thread_handler, (void *)&client_fd) != 0)
            p_error(E_T_CREATE);
        
        pthread_mutex_lock(&mutex_cpy_l_fd); //ensures that it is locked until copying client_fd
        pthread_mutex_unlock(&mutex_cpy_l_fd);
        
    }
    
    pthread_mutex_lock(&mutex_fd);
    nr_threads--;
    pthread_mutex_unlock(&mutex_fd);
    
    pthread_exit(NULL);
}

void* accept_remote_client_handler(void *args){
    struct sockaddr_in client_addr;
    socklen_t size_addr;
    pthread_t thread_id;
    remote remote_t;
    
    printf("\n[log] thread: threadID=%lu\n\t- was created to handle accept remote clients\n\n", pthread_self());
    
    pthread_mutex_lock(&mutex_fd);
    nr_threads++;
    pthread_mutex_unlock(&mutex_fd);
    
    /* Accept new connection from a new remote client */
    while((remote_t.fd = accept(r_in_sock_fd, (struct sockaddr *) &client_addr, &size_addr)) != -1){
        if(remote_t.fd == -1)
            p_error(E_ACCEPT);

        strcpy(remote_t.sin_addr, inet_ntoa(client_addr.sin_addr));
        
        printf("[log] thread: threadID=%lu\n\t- acceped connection from: %s\n\n", pthread_self(), remote_t.sin_addr);
        
        pthread_mutex_lock(&mutex_cpy_r_fd);
        
        // create a new thread to handle the communication with the new remote client
        if(pthread_create(&thread_id, NULL, remote_thread_handler, (void *)&remote_t) != 0)
            p_error(E_T_CREATE);
        
        pthread_mutex_lock(&mutex_cpy_r_fd); //ensures that it is locked until copying client_fd
        pthread_mutex_unlock(&mutex_cpy_r_fd);
    }
    
    pthread_mutex_lock(&mutex_fd);
    nr_threads--;
    pthread_mutex_unlock(&mutex_fd);
    
    pthread_exit(NULL);
}

void init(){
    int i;
    
    if ((all_client_fd = (client_fd*) malloc( INITIAL_NR_FD * sizeof(client_fd) )) == NULL)
        p_error(E_MALLOC);

    for(i = 0; i < NREGIONS; i++){
        memset(&clipboard[i], 0, sizeof(message_t)); // clear memory
        clipboard[i].region = i;
        
        if(pthread_rwlock_init(&rwlock_clip[i], NULL))
            p_error(E_RWLOCK);
    }
    
    unlink(SOCK_ADDRESS);

    printf(NAME);
    printf("[log] thread main: threadID:%lu\n", pthread_self());
}

void verifyInputArguments(int argc, char* argv[]){
    message_t m1;
    int i, port;
    char message[sizeof(message_t)];
    
    if(argc != 1 && argc != 4)
        inv(I_INPUT_USAGE);

    if(argc == 4){
        if(memcmp(argv[1], "-c", strlen(argv[1])) != 0)
            inv(I_OPTION);

        if(isValidIpAddress(argv[2]) == 0)
            inv(I_IP);
        
        port = atoi(argv[3]);
        if(port < MIN_PORT || port > MAX_PORT)
            inv(I_PORT);

        connect_server(argv[2], port);                                      //connect to the remote backup
    
        printf("\t- ready to receive initial data from remote backup\n\n");
        
        for(i = 0; i < NREGIONS; i++){
            if(recv(r_out_sock_fd, message, sizeof (message_t), 0) == -1)   //receive data from remote backup
                p_error(E_RECV);
                
            memcpy(&m1, message,  sizeof (message_t));                      //overwrite region
            clipboard[m1.region] = m1;
            printf("\t- recv[r]: region=%d | message=%s\n", m1.region, clipboard[m1.region].message);
        }

        remote_connection = 1;
        printf("\n");
    }

}

void open_local_socket(){
    struct sockaddr_un local_addr;

    /* Create socket */
    if((l_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        p_error(E_SOCKET);

    local_addr.sun_family = AF_UNIX;
    strcpy(local_addr.sun_path, SOCK_ADDRESS);

    /* Bind socket -> assigns the address to the socket referred to by the file descriptor sockfd. */
    if(bind(l_sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) == -1)
        p_error(E_BIND);

    /* Marks the socket as passive socket -> it will be used to accept incoming connection (0 -> success | -1 -> error)*/
    if(listen(l_sock_fd, NR_BACKLOG) == -1)
        p_error(E_LISTEN);
    
    printf("\t- local socket created and binded with name = %s\n", SOCK_ADDRESS);
}

void open_remote_socket(){
    struct sockaddr_in remote_addr;

    r_in_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (r_in_sock_fd == -1)
        p_error(E_SOCKET);
    
    srand(getpid());
    int length_of_range = MAX_PORT - MIN_PORT + 1;
    int port = rand()%length_of_range + MIN_PORT;

    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port= htons(port);
    remote_addr.sin_addr.s_addr= INADDR_ANY;

    if(bind(r_in_sock_fd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) == -1)
        p_error(E_BIND);
    
    if(listen(r_in_sock_fd, NR_BACKLOG) == -1)
        p_error(E_LISTEN);
    
    printf("\t- remote socket created and binded in port = %d\n", ntohs(remote_addr.sin_port));
}

void connect_server(char *ipAddress, int port){
    struct sockaddr_in server_addr;

    /* Create socket to the remote server */
    r_out_sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (r_out_sock_fd == -1)
        p_error(E_SOCKET);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(ipAddress, &server_addr.sin_addr);

    if(connect(r_out_sock_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr)) == -1)
        p_error(E_CONN);

    printf("\t- connected with success to remote backup at: %s:%d\n", inet_ntoa(server_addr.sin_addr), port);
}

void update_client_fds(client_fd client, int operation){
    int i, flag = 0;
    
    if(operation == ADD_FD){
        nr_threads++;
        if(nr_threads - 2 < INITIAL_NR_FD)
            if ((all_client_fd = (client_fd*) realloc(all_client_fd, (nr_threads-2) * sizeof(client_fd))) == NULL)
                p_error(E_REALLOC);
        
        all_client_fd[(nr_threads-2) - 1].fd = client.fd;
        all_client_fd[(nr_threads-2) - 1].type = client.type;
        
    }else{
        for(i = 0; i < (nr_threads-2); i++){
            if(!flag){
                if(all_client_fd[i].fd == client.fd)
                    flag = 1;
            }else{
                all_client_fd[i - 1].fd = all_client_fd[i].fd;
                all_client_fd[i - 1].type = all_client_fd[i].type;
            }
        }
        
        nr_threads--;
        if(nr_threads - 2 > INITIAL_NR_FD)
            if ((all_client_fd = (client_fd*) realloc(all_client_fd, (nr_threads-2) * sizeof(client_fd))) == NULL)
                p_error(E_REALLOC);
    }

}

int isValidIpAddress(char *ipAddress) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
}

void ctrl_c_callback_handler(int signum){
    printf("[sig] caught signal Ctr-C\n");
    secure_exit(0);
}

void p_error(char* msg){
    perror(msg);
    secure_exit(-1);
}

void inv(char* msg){
    printf(msg);
    secure_exit(-1);
}

void secure_exit(int status){ //falta proteger isto
    int i;
    
    for(i = 0; i < nr_threads - 2; i++){
        shutdown(all_client_fd[i].fd, SHUT_RDWR);
        close(all_client_fd[i].fd);
    }
    
    shutdown(l_sock_fd, SHUT_RDWR);
    close(l_sock_fd);
    shutdown(r_out_sock_fd, SHUT_RDWR);
    close(r_out_sock_fd);    
    shutdown(r_in_sock_fd, SHUT_RDWR);
    close(r_in_sock_fd);
    
    unlink(SOCK_ADDRESS);

    while(nr_threads);
    
    free(all_client_fd);
    
    for(i = 0; i < NREGIONS; i++)
        pthread_rwlock_destroy(&rwlock_clip[i]);
    
    pthread_mutex_destroy(&mutex_fd);
    pthread_mutex_destroy(&mutex_cpy_l_fd);
    pthread_mutex_destroy(&mutex_cpy_r_fd);
    
    printf("\n[!] exiting from clipboard\n\n");
    
    exit(status);
}
