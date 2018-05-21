#include "src/clipboard_int.h"
#define NAME "\n***CLIPBOARD***\n\n"

// mutexes
pthread_mutex_t mutex_nr_user    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_nr_threads = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cpy_l_fd   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cpy_r_fd   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_replicate  = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_data_cond  = PTHREAD_MUTEX_INITIALIZER;

// cond. variable
pthread_cond_t data_cond = PTHREAD_COND_INITIALIZER;

// rwlock
pthread_rwlock_t rwlock_clip[NREGIONS];

// vulnerable to race condition
clipboard_t clipboard[NREGIONS];
client_t *all_client_fd;
int nr_users = 0;
int nr_threads = 0;

// not vulnerable
int remote_connection = 0, l_sock_fd, r_out_sock_fd, r_in_sock_fd;

int main(int argc, char* argv[]){
    pthread_t thread_id;
    
    init();                                  // allocate memory and initialize mutexes
    signal(SIGINT, ctrl_c_callback_handler); // SIGINT CTRL-C
    open_local_socket();                     // open, bind and listen local socket
    open_remote_socket();                    // open, bind and listen remote socket
    verifyInputArguments(argc, argv);        // verify input arguments and create socket to remote backup if needded

    if(pthread_create(&thread_id, NULL, accept_local_client_handler, NULL) != 0)  // thread to handle accept local clients
        p_error(E_T_CREATE);
    
    if(pthread_create(&thread_id, NULL, accept_remote_client_handler, NULL) != 0) // thread to handle accept remote clients
        p_error(E_T_CREATE);

    // waits until all threads are created
    pthread_mutex_lock(&mutex_data_cond);
    pthread_cond_wait(&data_cond, &mutex_data_cond);
    pthread_mutex_unlock(&mutex_data_cond);

    printf("\n[log] thread main: threadID:%lu\n\t- all set!\n\n", pthread_self());

    // waits a signal to terminate
    pthread_mutex_lock(&mutex_data_cond);
    pthread_cond_wait(&data_cond, &mutex_data_cond);
    pthread_mutex_unlock(&mutex_data_cond);
    
    secure_exit(0);
    exit(0); // never happens
}

void* local_thread_handler(void* args){
    client_t client = *((client_t *) args);
    pthread_mutex_unlock(&mutex_cpy_l_fd);
    
    char message[sizeof(message_t)];
    void *message_clip = NULL;
    char clean_char = '\0';
    size_t size, received, aux;
    replicate_t replicate;
    pthread_t thread_id;
    message_t m1;
    
    pthread_mutex_lock(&mutex_nr_threads);
    nr_threads++;
    pthread_mutex_unlock(&mutex_nr_threads);
    
    pthread_mutex_lock(&mutex_nr_user);
    update_client_fds(client, ADD_FD);
    pthread_mutex_unlock(&mutex_nr_user);
    
    printf("[log] thread: threadID=%lu\n\t- was created to handle communication with the new local client\n\n", pthread_self());

    while(recv(client.fd, message, sizeof m1, 0) > 0) {
        memcpy(&m1, message, sizeof m1);
        
        printf("[log] thread: threadID=%lu\n", pthread_self());
        
        if ((message_clip = realloc(message_clip, m1.size)) == NULL)
            p_error(E_REALLOC);

        if(m1.operation == COPY) {
            received = 0;
            
            // clear clipboard region & overwrite region
            pthread_rwlock_wrlock(&rwlock_clip[m1.region]);
            memset(clipboard[m1.region].data, 0, clipboard[m1.region].size);
            if ((clipboard[m1.region].data = realloc(clipboard[m1.region].data, m1.size)) == NULL)
                p_error(E_REALLOC);
            clipboard[m1.region].size = m1.size;
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);
            
            while(received != m1.size){
                memset(message_clip, 0, m1.size);
                if((aux = recv(client.fd, message_clip, m1.size - received, 0)) == -1)
                    p_error(E_RECV);
                memcpy(clipboard[m1.region].data + received, message_clip, aux);
                received += aux;
            }
            
            pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
            printf("\t- recv[l]: region=%d | message=%s\n", m1.region, (char*)clipboard[m1.region].data);
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);

            // thread to handle replication of this region for all remote clipboards execpt the one that asks the copy
            replicate.client = client;
            replicate.message = m1;
            replicate.data = NULL;
            if ((replicate.data = realloc(replicate.data, m1.size)) == NULL) p_error(E_REALLOC);
            memcpy(replicate.data, message_clip, m1.size);
            
            pthread_mutex_lock(&mutex_replicate);
            if(pthread_create(&thread_id, NULL, replicate_copy_cmd, (void *)&replicate) != 0) 
                p_error(E_T_CREATE);
            
            pthread_mutex_lock(&mutex_replicate); // ensures that it is locked until copying client.fd and the message
            pthread_mutex_unlock(&mutex_replicate);
            

        }else if(m1.operation == PASTE){
            
            pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
            size = clipboard[m1.region].size;
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);
            
            if((int)size){
                // copy clipboard[m1.region] to message with the size required
                pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
                memcpy(message_clip, clipboard[m1.region].data, m1.size);
                pthread_rwlock_unlock(&rwlock_clip[m1.region]);
                
                // send message to socket
                if(write(client.fd, message_clip, m1.size) == -1)
                    p_error(E_WRITE);
                
                pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
                printf("\t- sent[l]: region=%d | message=%s\n\n", m1.region, (char*)clipboard[m1.region].data);
                pthread_rwlock_unlock(&rwlock_clip[m1.region]);
            }else{
                if(write(client.fd, &clean_char, m1.size) == -1)
                    p_error(E_WRITE);
                printf("\t- sent[l]: region=%d | message=\n\n", m1.region);
            }
        }
    }
    printf("[log] thread: threadID=%lu\n\t- closing connection and exiting.\n\n", pthread_self());

    // when it receives EOF closes connection
    close(client.fd);
    
    free(message_clip);
    free(replicate.data);
    
    pthread_mutex_lock(&mutex_nr_user);
    update_client_fds(client, RMV_FD);
    pthread_mutex_unlock(&mutex_nr_user);
    
    pthread_mutex_lock(&mutex_nr_threads);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex_data_cond);
    pthread_mutex_unlock(&mutex_nr_threads);
    
    //will exit the thread that calls it.
    pthread_exit(NULL);
}

void* remote_thread_handler(void *args){
    client_t client = *((client_t *) args);
    pthread_mutex_unlock(&mutex_cpy_r_fd);
    
    char message[sizeof(message_t)];
    char clean_char = '\0';
    void *message_clip = NULL;
    int region;
    size_t size, received, aux;
    replicate_t replicate;
    pthread_t thread_id;
    message_t m1;
    
    pthread_mutex_lock(&mutex_nr_threads);
    nr_threads++;
    pthread_mutex_unlock(&mutex_nr_threads);
    
    pthread_mutex_lock(&mutex_nr_user);
    update_client_fds(client, ADD_FD);
    pthread_mutex_unlock(&mutex_nr_user);
    
    printf("[log] thread: threadID=%lu\n\t- was created to handle communication with the new remote client: %s\n", pthread_self(), client.sin_addr);

    if(client.fd != r_out_sock_fd){
        printf("\n");
        for(region = 0; region < NREGIONS; region++){
            
            pthread_rwlock_rdlock(&rwlock_clip[region]);
            size = clipboard[region].size;
            pthread_rwlock_unlock(&rwlock_clip[region]);

            m1.operation = COPY;
            m1.region = region;
            m1.size = size;
            
            memcpy(message, &m1, sizeof m1);

            if(write(client.fd, message, sizeof m1) == -1) //send message to clipboard
                p_error(E_WRITE);
            
            if(!(int)size){
                if(write(client.fd, &clean_char, m1.size) == -1)
                    p_error(E_WRITE);
                printf("\t- sent[r]: region=%d | message=\n", region);
                continue;
            }
            
            if ((message_clip = realloc(message_clip, size)) == NULL)
                p_error(E_REALLOC);
        
            //copy clipboard[region] to message_clip
            pthread_rwlock_rdlock(&rwlock_clip[region]);
            memcpy(message_clip, clipboard[region].data, size);
            pthread_rwlock_unlock(&rwlock_clip[region]);
            
            if(write(client.fd, message_clip, size) == -1) //send message to clipboard
                p_error(E_WRITE);
            
            printf("\t- sent[r]: region=%d | message=%s\n", region, (char*)message_clip);
        }
        printf("\n");
    }

    while(recv(client.fd, message, sizeof(message_t), 0) > 0){
        memcpy(&m1, message, sizeof m1);
        
        if(!(int)m1.size){
            printf("\t- recv[r]: region=%d | message=\n", m1.region);
            continue;
        }
        
        printf("[log] thread: threadID=%lu \n", pthread_self());
        
        if ((message_clip = realloc(message_clip, m1.size)) == NULL)
            p_error(E_REALLOC);
        
        // clear clipboard region & overwrite region
        pthread_rwlock_wrlock(&rwlock_clip[m1.region]);
        memset(clipboard[m1.region].data, 0, clipboard[m1.region].size);
        if ((clipboard[m1.region].data = realloc(clipboard[m1.region].data, m1.size)) == NULL)
            p_error(E_REALLOC);
        clipboard[m1.region].size = m1.size;
        pthread_rwlock_unlock(&rwlock_clip[m1.region]);
        
        received = 0;
        while(received != m1.size){
            memset(message_clip, 0, m1.size);
            if((aux = recv(client.fd, message_clip, m1.size - received, 0)) == -1)
                p_error(E_RECV);
            memcpy(clipboard[m1.region].data + received, message_clip, aux);
            received += aux;
        }

        //print to terminal the region
        pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
        printf("\t- recv[r]: region=%d | message=%s\n", m1.region, (char*)clipboard[m1.region].data);
        pthread_rwlock_unlock(&rwlock_clip[m1.region]);
        
        // thread to handle replication of this region for all remote clipboards execpt the one that asks the copy
        replicate.client = client;
        replicate.message = m1;
        replicate.data = NULL;
        if ((replicate.data = realloc(replicate.data, m1.size)) == NULL) p_error(E_REALLOC);
        memcpy(replicate.data, message_clip, m1.size);
        
        pthread_mutex_lock(&mutex_replicate);
        if(pthread_create(&thread_id, NULL, replicate_copy_cmd, (void *)&replicate) != 0) 
            p_error(E_T_CREATE);
        
        pthread_mutex_lock(&mutex_replicate); // ensures that it is locked until copying client.fd and the message
        pthread_mutex_unlock(&mutex_replicate);
        
    }
    
    printf("[log] thread: threadID=%lu\n\t- closing connection and exiting.\n\n", pthread_self());

    // when it receives EOF closes connection
    close(client.fd);
    
    free(message_clip);
    free(replicate.data);
    
    pthread_mutex_lock(&mutex_nr_user);
    update_client_fds(client, RMV_FD);
    pthread_mutex_unlock(&mutex_nr_user);
    
    pthread_mutex_lock(&mutex_nr_threads);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex_data_cond);
    pthread_mutex_unlock(&mutex_nr_threads);

    //will exit the thread that calls it.
    pthread_exit(NULL);
}

void* accept_local_client_handler(void *args){
    struct sockaddr_un client_addr;
    socklen_t size_addr = sizeof(struct sockaddr);
    pthread_t thread_id;
    client_t client;
    
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sun_family = AF_UNIX;
    strcpy(client_addr.sun_path, SOCK_ADDRESS);
    
    printf("\n[log] thread: threadID=%lu\n\t- was created to handle accept local clients\n", pthread_self());
    
    
    client.type = LOCAL;
    
    pthread_mutex_lock(&mutex_nr_threads);
    nr_threads++;
    //signal if all initial threads have been created
    if(remote_connection && nr_threads == 3)
        pthread_cond_signal(&data_cond);
    else if(!remote_connection && nr_threads == 2)
        pthread_cond_signal(&data_cond);
    pthread_mutex_unlock(&mutex_nr_threads);

    /* Accept new connection from a new local client */
    while((client.fd = accept(l_sock_fd, (struct sockaddr *) &client_addr, &size_addr)) != -1){
        if(client.fd == -1)
            p_error(E_ACCEPT);
        
        printf("[log] thread: threadID=%lu\n\t- acceped connection from: %s\n\n", pthread_self(), client_addr.sun_path);
        
        pthread_mutex_lock(&mutex_cpy_l_fd);
        
        // create a new thread to handle the communication with the new local client
        if(pthread_create(&thread_id, NULL, local_thread_handler, (void *)&client) != 0)
            p_error(E_T_CREATE);
        
        pthread_mutex_lock(&mutex_cpy_l_fd); // ensures that it is locked until copying client
        pthread_mutex_unlock(&mutex_cpy_l_fd);
        
    }
    
    printf("[log] thread: threadID=%lu\n\t- exiting.\n\n", pthread_self());
    
    pthread_mutex_lock(&mutex_nr_threads);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex_data_cond);
    pthread_mutex_unlock(&mutex_nr_threads);
    
    pthread_exit(NULL);
}

void* accept_remote_client_handler(void *args){
    struct sockaddr_in client_addr;
    socklen_t size_addr = sizeof(struct sockaddr);
    pthread_t thread_id;
    client_t client;
    
    printf("\n[log] thread: threadID=%lu\n\t- was created to handle accept remote clients\n", pthread_self());
    
    client.type = REMOTE_C;
    
    pthread_mutex_lock(&mutex_nr_threads);
    nr_threads++;
    //signal if all initial threads have been created
    if(remote_connection && nr_threads == 3)
        pthread_cond_signal(&data_cond);
    else if(!remote_connection && nr_threads == 2)
        pthread_cond_signal(&data_cond);
    pthread_mutex_unlock(&mutex_nr_threads);

    // Accept new connection from a new remote client
    while((client.fd = accept(r_in_sock_fd, (struct sockaddr *) &client_addr, &size_addr)) != -1){
        if(client.fd == -1)
            p_error(E_ACCEPT);

        strcpy(client.sin_addr, inet_ntoa(client_addr.sin_addr));
        
        printf("[log] thread: threadID=%lu\n\t- acceped connection from: %s\n\n", pthread_self(), client.sin_addr);
        
        pthread_mutex_lock(&mutex_cpy_r_fd);
        
        // create a new thread to handle the communication with the new remote client
        if(pthread_create(&thread_id, NULL, remote_thread_handler, (void *)&client) != 0)
            p_error(E_T_CREATE);
        
        pthread_mutex_lock(&mutex_cpy_r_fd); //ensures that it is locked until copying client
        pthread_mutex_unlock(&mutex_cpy_r_fd);
    }
    
    printf("[log] thread: threadID=%lu\n\t- exiting.\n\n", pthread_self());
    
    pthread_mutex_lock(&mutex_nr_threads);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex_data_cond);
    pthread_mutex_unlock(&mutex_nr_threads);
    
    pthread_exit(NULL);
}


void* replicate_copy_cmd(void *args){
    replicate_t replicate = *((replicate_t *) args);
    pthread_mutex_unlock(&mutex_replicate);

    char message[sizeof(message_t)];
    int i, n_user;
    
    pthread_mutex_lock(&mutex_nr_user);
    n_user = nr_users;
    pthread_mutex_unlock(&mutex_nr_user);

    memcpy(message, &replicate.message, sizeof(message_t));
    
    pthread_mutex_lock(&mutex_nr_threads);
    nr_threads++;
    pthread_mutex_unlock(&mutex_nr_threads);

    printf("\n[log] thread: threadID=%lu\n\t- was created to handle replication of region %d\n", pthread_self(), replicate.message.region);
    
    client_t *all_client_fd_aux;    
    if ((all_client_fd_aux = (client_t*) malloc(n_user*sizeof(client_t) )) == NULL)
        p_error(E_MALLOC);
    
    pthread_mutex_lock(&mutex_nr_user);    
    memcpy(all_client_fd_aux, all_client_fd, n_user*sizeof(client_t)); 
    pthread_mutex_unlock(&mutex_nr_user);
    
    for(i = 0; i < n_user; i++){
        if(all_client_fd_aux[i].type >= REMOTE_C){
            if((all_client_fd_aux[i].fd != replicate.client.fd && replicate.client.type >= REMOTE_C) || replicate.client.type == LOCAL){

                if(write(all_client_fd_aux[i].fd, message, sizeof(message_t)) == -1)
                    p_error(E_WRITE);
                
                if(write(all_client_fd_aux[i].fd, replicate.data, replicate.message.size) == -1)
                    p_error(E_WRITE);
                
                printf("\t- sent[r]: region=%d | message=%s\n", replicate.message.region, (char*)replicate.data);
            }
        }
    }
    
    free(all_client_fd_aux);
    
    printf("\t- exiting.\n\n");
    
    pthread_mutex_lock(&mutex_nr_threads);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex_data_cond);
    pthread_mutex_unlock(&mutex_nr_threads);

    pthread_exit(NULL);
}

void init(){
    int i;
    
    if ((all_client_fd = (client_t*) malloc( INITIAL_NR_FD * sizeof(client_t) )) == NULL)
        p_error(E_MALLOC);
    
    for(i = 0; i < NREGIONS; i++){
        clipboard[i].data = NULL;
        clipboard[i].size = 0;
        
        if(pthread_rwlock_init(&rwlock_clip[i], NULL))
            p_error(E_RWLOCK);
    }
    
    unlink(SOCK_ADDRESS);

    printf(NAME);
    printf("[log] thread main: threadID:%lu\n", pthread_self());
}

void verifyInputArguments(int argc, char* argv[]){
    pthread_t thread_id;
    client_t remote;
    int port;
    
    if(argc != 1 && argc != 4)
        inv(I_INPUT_USAGE);

    if(argc == 4){
        if(memcmp(argv[1], "-c", strlen(argv[1])) != 0)
            inv(I_OPTION);

        if(!isValidIpAddress(argv[2]))
            inv(I_IP);
        
        port = atoi(argv[3]);
        if(port < MIN_PORT || port > MAX_PORT)
            inv(I_PORT);

        //connect to the remote backup
        remote = connect_server(argv[2], port);
        remote_connection = 1;
        
        printf("\n");
        pthread_mutex_lock(&mutex_cpy_r_fd);
        // create a new thread to handle the communication with the new remote client
        if(pthread_create(&thread_id, NULL, remote_thread_handler, (void *)&remote) != 0)
            p_error(E_T_CREATE);
        pthread_mutex_lock(&mutex_cpy_r_fd); //ensures that it is locked until copying client
        pthread_mutex_unlock(&mutex_cpy_r_fd);
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
    int range = MAX_PORT - MIN_PORT + 1;
    int port = rand()%range + MIN_PORT;

	memset(&remote_addr, 0, sizeof(struct sockaddr));
    
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port= htons(port);
    remote_addr.sin_addr.s_addr= INADDR_ANY;

    if(bind(r_in_sock_fd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1)
        p_error(E_BIND);
    
    if(listen(r_in_sock_fd, NR_BACKLOG) == -1)
        p_error(E_LISTEN);
    
    printf("\t- remote socket created and binded in port = %d\n", ntohs(remote_addr.sin_port));
}

client_t connect_server(char *ipAddress, int port){
    client_t remote;
    struct sockaddr_in server_addr;

    /* Create socket to the remote server */
    r_out_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (r_out_sock_fd == -1)
        p_error(E_SOCKET);

	memset(&server_addr, 0, sizeof(struct sockaddr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(ipAddress, &server_addr.sin_addr);

    if(connect(r_out_sock_fd, (const struct sockaddr *) &server_addr, sizeof(struct sockaddr)) == -1)
        p_error(E_CONN);

    printf("\t- connected with success to %s:%d\n", inet_ntoa(server_addr.sin_addr), port);

    remote.fd = r_out_sock_fd;
    remote.type = REMOTE_S;
    memcpy(remote.sin_addr, inet_ntoa(server_addr.sin_addr), sizeof(remote.sin_addr));    
    
    return remote;
}

void update_client_fds(client_t client, int operation){
    int i, flag = 0;
    
    if(operation == ADD_FD){
        if(++nr_users > INITIAL_NR_FD)
            if ((all_client_fd = (client_t*) realloc(all_client_fd, nr_users * sizeof(client_t))) == NULL)
                p_error(E_REALLOC);
        
        all_client_fd[nr_users - 1] = client;
        
    }else{
        for(i = 0; i < nr_users; i++){
            if(!flag){
                if(all_client_fd[i].fd == client.fd)
                    flag = 1;
            }else{
                all_client_fd[i - 1] = all_client_fd[i];
            }
        }
        
        if(--nr_users > INITIAL_NR_FD)
            if ((all_client_fd = (client_t*) realloc(all_client_fd, nr_users * sizeof(client_t))) == NULL)
                p_error(E_REALLOC);
    }
}

int isValidIpAddress(char *ipAddress) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
}

void ctrl_c_callback_handler(int signum){
    printf("[sig] caught signal Ctr-C\n\n");
    pthread_cond_signal(&data_cond);
}

void p_error(char* msg){
    perror(msg);
    secure_exit(-1);
}

void inv(char* msg){
    printf(msg);
    secure_exit(-1);
}

void secure_exit(int status){
    int i;
    
    pthread_mutex_lock(&mutex_data_cond);
    for(i = 0; i < nr_users; i++){
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

    //waits until every thread are closed
    pthread_mutex_lock(&mutex_data_cond);
    pthread_mutex_unlock(&mutex_data_cond);
    
    free(all_client_fd);
    
    for(i = 0; i < NREGIONS; i++){
        pthread_rwlock_destroy(&rwlock_clip[i]);
        free(clipboard[i].data);
    }
    
    pthread_mutex_destroy(&mutex_nr_user);
    pthread_mutex_destroy(&mutex_nr_threads);
    pthread_mutex_destroy(&mutex_cpy_l_fd);
    pthread_mutex_destroy(&mutex_cpy_r_fd);
    pthread_mutex_destroy(&mutex_replicate);
    pthread_mutex_destroy(&mutex_data_cond);
    pthread_cond_destroy (&data_cond);

    printf("\n[!] exiting from clipboard\n\n");
    
    exit(status);
}
