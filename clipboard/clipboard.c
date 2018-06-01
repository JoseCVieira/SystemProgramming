#include "src/clipboard_int.h"

#define DEBUG

// mutexes
pthread_mutex_t mutex[] = {
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER, 
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};

// cond. variable
pthread_cond_t data_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t data_cond_wait = PTHREAD_COND_INITIALIZER;

// rwlock
pthread_rwlock_t rwlock_clip[NREGIONS];

// vulnerable to race condition
clipboard_t clipboard[NREGIONS];
client_t *all_client_fd;
int nr_users = 0;
int nr_threads = 0;

// not vulnerable
int remote_connection = 0, l_sock_fd, r_out_sock_fd, r_in_sock_fd, t_in_sock_fd, t_out_sock_fd;
struct sockaddr_in *top_clip_address;
client_t top_clip_client;
char has_top = 0;

int main(int argc, char* argv[]){
    pthread_t thread_id;
    
    init();
    signal(SIGINT, ctrl_c_callback_handler);        // SIGINT CTRL-C
    signal(SIGPIPE, broken_pipe_callback_handler);  // SIGPIPE broken pipe
    open_local_socket();                            // open, bind and listen local socket
    open_remote_socket();                           // open, bind and listen remote socket
    verifyInputArguments(argc, argv);               // verify input arguments and create socket to remote backup if needded

    // thread to handle accept local clients
    if(pthread_create(&thread_id, NULL, accept_local_client_handler, NULL) != 0){
        perror(E_T_CREATE);
        secure_exit(1);
    }
    // detach thread so when it terminates, its resources are automatically released
    if (pthread_detach(thread_id)){
        perror(E_T_DETACH);
        secure_exit(1);
    }
    
    // thread to handle accept remote clients
    if(pthread_create(&thread_id, NULL, accept_remote_client_handler, NULL) != 0){
        perror(E_T_CREATE);
        secure_exit(1);
    }
    // detach thread so when it terminates, its resources are automatically released
    if (pthread_detach(thread_id)){
        perror(E_T_DETACH);
        secure_exit(1);
    }
    
    if(!remote_connection){        
        if(pthread_create(&thread_id, NULL, accept_timestamp_client_handler, NULL) != 0){
            perror(E_T_CREATE);
            secure_exit(1);
        }
        // detach thread so when it terminates, its resources are automatically released
        if (pthread_detach(thread_id)){
            perror(E_T_DETACH);
            secure_exit(1);
        }
    }

    // waits until all initial threads are created
    pthread_mutex_lock(&mutex[M_DATA_C]);
    pthread_cond_wait(&data_cond, &mutex[M_DATA_C]);
    pthread_mutex_unlock(&mutex[M_DATA_C]);

    printf("\n[log] thread main: threadID:%lu\n\t- all set!\n\n", pthread_self());

    // waits a signal to terminate
    pthread_mutex_lock(&mutex[M_DATA_C]);
    pthread_cond_wait(&data_cond, &mutex[M_DATA_C]);
    pthread_mutex_unlock(&mutex[M_DATA_C]);

    secure_exit(0);
    return 0;
}

// thread to handle the communication with local clients
// when some error occur, the communication with the client is closed
// and the distributed clipboard continues
void* local_thread_handler(void* args){
    client_t client = *((client_t *) args);
    pthread_mutex_unlock(&mutex[M_CPY_L]);
    
    char message_timestamp[sizeof(timestamp_t)];
    char message[sizeof(message_t)];
    char clean_char = '\0', flag;
    size_t size, received, aux;
    void *message_clip = NULL;
    int i, return_val, error;
    replicate_t replicate;
    timestamp_t timestamp;
    pthread_t thread_id;
    message_t m1, m2;
    
    replicate.data = NULL;

    // update the list of clients with the new local client
    return_val = WAIT;
    while(return_val == WAIT){
        
        pthread_mutex_lock(&mutex[M_USER]);
        return_val = update_client_fds(client, ADD_FD);
        pthread_mutex_unlock(&mutex[M_USER]);
        
        if(return_val == WAIT){
            pthread_mutex_lock(&mutex[M_WAIT_FD]);
            pthread_cond_wait(&data_cond_wait, &mutex[M_WAIT_FD]);
            pthread_mutex_unlock(&mutex[M_WAIT_FD]);
        }
    }
    
    // increment number of active threads
    pthread_mutex_lock(&mutex[M_THREAD]);
    nr_threads++;
    pthread_mutex_unlock(&mutex[M_THREAD]);
    
    #ifdef DEBUG
    printf("[log] thread: threadID=%lu\n\t- was created to handle communication with the new local client\n\n", pthread_self());
    #endif

    // waits until a new message arrives from the local client or break loop when
    // it receives a 0 (end of file)
    while(recv(client.fd, message, sizeof m1, 0) > 0) {
        
        // convert the received data into a well defined struct
        // that contains the operation to make, the size os the
        // data to receive and the respective region
        memcpy(&m1, message, sizeof m1);
        
        #ifdef DEBUG
        printf("[log] thread: threadID=%lu\n", pthread_self());
        #endif
        
        flag = 1;
        if ((message_clip = realloc(message_clip, m1.size)) == NULL){
            perror(E_REALLOC); // print the error
            flag = 0;
            if(write(client.fd, &flag, 1) <= 0) { // tell client that an error occurred
                perror(E_WRITE);
                break;
            }
            continue; //will back to recv state
        }
        
        int s;
        if(m1.operation == COPY) {
            
            if(remote_connection){
                m2.operation = TMSTAMP;
                m2.size = sizeof(timestamp_t);
                
                memcpy(message, &m2, sizeof m2);
                
                // sends info message to clipboard
                if(write(top_clip_client.fd, message, sizeof m2) <= 0)
                    p_error(E_WRITE); // cannot communicate with the top remote then exits
                
                if((s=recv(top_clip_client.fd, message_timestamp, sizeof(timestamp_t), 0)) <= 0)
                    p_error(E_RECV); // cannot communicate with the top remote then exits
                
                memcpy(&timestamp, message_timestamp, sizeof(message_timestamp));
            }else
                timestamp = get_timestamp();
            
            // clear clipboard region and reallocate memory of the region
            pthread_rwlock_wrlock(&rwlock_clip[m1.region]);
            clipboard[m1.region].data = realloc(clipboard[m1.region].data, m1.size);
            
            if (clipboard[m1.region].data == NULL)
                flag = 0;
            
            printf("size of = %d\n", s);
            printf("\t-old ts => %d:%d:%d:%lu\n", clipboard[m1.region].ts.tm_struct.tm_hour, clipboard[m1.region].ts.tm_struct.tm_min, clipboard[m1.region].ts.tm_struct.tm_sec, clipboard[m1.region].ts.tv.tv_usec);
            printf("\t-new ts => %d:%d:%d:%lu\n", timestamp.tm_struct.tm_hour, timestamp.tm_struct.tm_min, timestamp.tm_struct.tm_sec, timestamp.tv.tv_usec);
            
            if(!compare_timestamp(clipboard[m1.region].ts, timestamp))
                flag = -1;
            else
                clipboard[m1.region].ts = timestamp;
            
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);
            
            printf("3 flag = %d\n", flag);
            
            if (flag <= 0){ // if could not allocate memory or, the timestamp is lower than the present one, then
                if(!flag)
                    perror(E_REALLOC);
                flag = 0;
                
                if(write(client.fd, &flag, 1) <= 0) { // inform the client that it is not possible to copy
                    perror(E_WRITE);
                    break; // if some error occurred closes client
                }
                continue;
            }
            
            // write to client success or error when allocating memory
            if(write(client.fd, &flag, 1) <= 0) {
                perror(E_WRITE);
                break;
            }
            
            // receive data until size is the same as the previous size received
            received = 0;
            while(received < m1.size){
                if((aux = recv(client.fd, message_clip + received, m1.size - received, 0)) <= 0){
                    perror(E_RECV); // print the error and close the client
                    received = -1;
                    break;
                }
                received += aux;
            }
            if(received == -1)
                break; // if some error occurred closes client
            
            pthread_rwlock_wrlock(&rwlock_clip[m1.region]);
            memset(clipboard[m1.region].data, 0, clipboard[m1.region].size);
            clipboard[m1.region].size = m1.size;
            memcpy(clipboard[m1.region].data, message_clip, m1.size);
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);
            
            #ifdef DEBUG
            pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
            printf("\t-timestamp received => %d:%d:%d:%lu\n", timestamp.tm_struct.tm_hour, timestamp.tm_struct.tm_min, timestamp.tm_struct.tm_sec, timestamp.tv.tv_usec);
            printf("\t- recv[l]: region=%d | message=%s\n", m1.region, (char*)clipboard[m1.region].data);
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);
            #endif

            // thread to handle replication of this region for all remote clipboards 
            // except the one that asks the copy and for some local app that is locked
            // in a wait state
            replicate.client = client;
            replicate.message = m1;
            replicate.data = message_clip;
            
            // thread to handle replication
            pthread_mutex_lock(&mutex[M_REPL]);
            error = 0;
            if(pthread_create(&thread_id, NULL, replicate_copy_cmd, (void *)&replicate) != 0){
                error = 1;
                perror(E_T_CREATE);
            }
            // detach thread so when it terminates, its resources are automatically released
            if(!error) {
                if (pthread_detach(thread_id)){
                    error = 1;
                    perror(E_T_DETACH); // print the error
                }
            }
            
            // ensures that it is locked until copying replicate struct
            if(!error)
                pthread_mutex_lock(&mutex[M_REPL]);
            else 
                break; // if some error occurred close the client
            
            pthread_mutex_unlock(&mutex[M_REPL]);
            
            replicate.data = NULL;
            free(message_clip);
            message_clip = NULL;
            

        }else if(m1.operation == PASTE){
            
            // write to client success or error when allocating memory
            if(write(client.fd, &flag, 1) <= 0) {
                perror(E_WRITE);
                break;
            }
            
            pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
            size = clipboard[m1.region].size;
            pthread_rwlock_unlock(&rwlock_clip[m1.region]);
            
            if((int)size){
                // copy clipboard[m1.region] to message with the required size
                pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
                memcpy(message_clip, clipboard[m1.region].data, m1.size);
                pthread_rwlock_unlock(&rwlock_clip[m1.region]);
                
                // send the data present in the asked region to the socket
                if(write(client.fd, message_clip, m1.size) <= 0) {
                    perror(E_WRITE); // print the error and close the client
                    break;
                }
                
                #ifdef DEBUG
                pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
                printf("\t- sent[l]: region=%d | message=%s\n\n", m1.region, (char*)clipboard[m1.region].data);
                pthread_rwlock_unlock(&rwlock_clip[m1.region]);
                #endif
            }else{
                // nothing to send. just send a '\0' character
                if(write(client.fd, &clean_char, 1) <= 0) {
                    perror(E_WRITE); // print the error and close the client
                    break;
                }
                #ifdef DEBUG
                printf("\t- sent[l]: region=%d | message=\n\n", m1.region);
                #endif
            }
        }else if(m1.operation == WAIT){
            
            #ifdef DEBUG
            printf("\t- recv[l]: region=%d | wait signal\n\n", m1.region);
            #endif
            pthread_mutex_lock(&mutex[M_USER]);
            
            // searches in the list of all clients of this clipboard and put the client
            // as waiting mode, so in the next overwrite of the received region
            // the replication is sent to him too
            for(i = 0; i < nr_users; i++){
                if(all_client_fd[i].fd == client.fd){
                    all_client_fd[i].wait = m1.region;
                    all_client_fd[i].wait_size = m1.size;
                    break;
                }
            }
            pthread_mutex_unlock(&mutex[M_USER]);
        }else
            break;
        
    }
    #ifdef DEBUG
    printf("[log] thread: threadID=%lu\n\t- closing connection and exiting.\n\n", pthread_self());
    #endif

    free(message_clip);
    free(replicate.data);
    
    // remove client from the list of all clients
    pthread_mutex_lock(&mutex[M_USER]);
    // when it receives EOF closes connection
    shutdown(client.fd, SHUT_RDWR);
    close(client.fd);
    
    update_client_fds(client, RMV_FD);
    pthread_mutex_unlock(&mutex[M_USER]);
    
    // decrement the number of active threads, and if it is zero
    // send a signal to the condition variable so the program can exit safely
    pthread_mutex_lock(&mutex[M_THREAD]);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex[M_DATA_C]);
    pthread_mutex_unlock(&mutex[M_THREAD]);
    
    //will exit the thread that calls it.
    pthread_exit(NULL);
}

// thread to handle the communication with other clipboards
// when some error occur, the communication with the client is closed
// and the distributed clipboard continues
void* remote_thread_handler(void *args){
    client_t client = *((client_t *) args);
    pthread_mutex_unlock(&mutex[M_CPY_R]);
    
    char message[sizeof(message_t)];
    void *message_clip = NULL;
    int region, return_val, error = 0;
    size_t size, received, aux;
    char send_top[sizeof(struct sockaddr_in)];
    replicate_t replicate;
    pthread_t thread_id;
    message_t m1;
    
    replicate.data = NULL;
    
    // sends top clipboard address
    if(client.type != REMOTE_S){
        memcpy(send_top, top_clip_address, sizeof(struct sockaddr_in));
        if(write(client.fd, send_top, sizeof send_top) == -1){
            perror(E_WRITE);
            shutdown(client.fd, SHUT_RDWR);
            close(client.fd);
            pthread_exit(NULL);
        }
    }
    
    // update the list of clients with the new remote client
    return_val = WAIT;
    while(return_val == WAIT){
        pthread_mutex_lock(&mutex[M_USER]);
        return_val = update_client_fds(client, ADD_FD);
        pthread_mutex_unlock(&mutex[M_USER]);
        
        if(return_val == WAIT){
            pthread_mutex_lock(&mutex[M_WAIT_FD]);
            pthread_cond_wait(&data_cond_wait, &mutex[M_WAIT_FD]);
            pthread_mutex_unlock(&mutex[M_WAIT_FD]);
        }
    }
    
    // increment number of active threads
    pthread_mutex_lock(&mutex[M_THREAD]);
    nr_threads++;
    pthread_mutex_unlock(&mutex[M_THREAD]);
    
    #ifdef DEBUG
    printf("[log] thread: threadID=%lu\n\t- was created to handle communication with the new remote client: %s\n", pthread_self(), client.sin_addr);
    #endif
    
    // if some other clipboard have connected to this clipboard, then
    // this one, sends all the current clipboard data from all regions
    if(client.fd != r_out_sock_fd){
        for(region = 0; region < NREGIONS; region++){
            pthread_rwlock_rdlock(&rwlock_clip[region]);
            size = clipboard[region].size;
            pthread_rwlock_unlock(&rwlock_clip[region]);
            
            // if this region have no data yet, then do not send anything
            if(!(int)size)
                continue;

            m1.operation = COPY;
            m1.region = region;
            m1.size = size;
            
            if((message_clip = realloc(message_clip, size)) == NULL){
                perror(E_REALLOC);
                error = 1;
                break;
            }
        
            // copy clipboard's data from this region to message_clip
            pthread_rwlock_rdlock(&rwlock_clip[region]);
            memcpy(message_clip, clipboard[region].data, size);
            pthread_rwlock_unlock(&rwlock_clip[region]);
            
            memcpy(message, &m1, sizeof m1);

            // sends info message to clipboard
            if(write(client.fd, message, sizeof m1) == -1){
                perror(E_WRITE);
                error = 1;
                break;
            }
            
            // send the actual data to the new clipboard
            if(write(client.fd, message_clip, size) == -1){
                perror(E_WRITE);
                error = 1;
                break;
            }
            
            #ifdef DEBUG
            printf("\t- sent[r]: region=%d | message=%s\n", region, (char*)message_clip);
            #endif
        }
        if(error){
            perror(E_WRITE);
            shutdown(client.fd, SHUT_RDWR);
            close(client.fd);
            pthread_exit(NULL);
            free(message_clip);
        }
    }

    // waits until a new message arrives from the local client or break loop when
    // it receives a 0 (end of file)
    while(recv(client.fd, message, sizeof m1, 0) > 0){
        // convert the received data into a well defined struct
        // that contains the operation to make, the size os the
        // data to receive and the respective region
        memcpy(&m1, message, sizeof m1);
        
        #ifdef DEBUG
        printf("[log] thread: threadID=%lu \n", pthread_self());
        #endif
        
        // realloc message to receive all data of this region
        if ((message_clip = realloc(message_clip, m1.size)) == NULL){
            perror(E_REALLOC);
            continue;
        }
        
        // clear clipboard region and reallocate memory of the region
        pthread_rwlock_wrlock(&rwlock_clip[m1.region]);
        clipboard[m1.region].data = realloc(clipboard[m1.region].data, m1.size);
        
        error = 0;
        if(clipboard[m1.region].data == NULL)
            error = 1;
        pthread_rwlock_unlock(&rwlock_clip[m1.region]);
        
        if(error){
            perror(E_REALLOC);
            continue;
        }
        
        // receive data until size is the same as the previous size received 
        received = 0;
        while(received < m1.size){
            if((aux = recv(client.fd, message_clip + received, m1.size - received, 0)) <= 0){
                perror(E_RECV); // print the error and close the client
                received = -1;
                break;
            }
            received += aux;
        }
        
        if(received == -1)
            break;
        
        pthread_rwlock_wrlock(&rwlock_clip[m1.region]);
        memset(clipboard[m1.region].data, 0, clipboard[m1.region].size);
        clipboard[m1.region].size = m1.size;
        memcpy(clipboard[m1.region].data, message_clip, m1.size);
        pthread_rwlock_unlock(&rwlock_clip[m1.region]);

        #ifdef DEBUG
        pthread_rwlock_rdlock(&rwlock_clip[m1.region]);
        printf("\t- recv[r]: region=%d | message=%s\n", m1.region, (char*)clipboard[m1.region].data);
        pthread_rwlock_unlock(&rwlock_clip[m1.region]);
        #endif
        
        // thread to handle replication of this region for all remote clipboards
        // except the one that asks the copy and for some local app that is locked
        // in a wait state
        replicate.client = client;
        replicate.message = m1;
        replicate.data = message_clip;
        
        // thread to handle replication
        pthread_mutex_lock(&mutex[M_REPL]);
        error = 0;
        if(pthread_create(&thread_id, NULL, replicate_copy_cmd, (void *)&replicate) != 0){
            error = 1;
            perror(E_T_CREATE);
            
        }
        // detach thread so when it terminates, its resources are automatically released
        if(!error) {
            if (pthread_detach(thread_id)){
                error = 1;
                perror(E_T_DETACH); // print the error
            }
        }
        
        // ensures that it is locked until copying replicate struct
        if(!error)
            pthread_mutex_lock(&mutex[M_REPL]);
        else{
            pthread_mutex_unlock(&mutex[M_REPL]);
            break; // if some error occurred close the client
        }
        
        pthread_mutex_unlock(&mutex[M_REPL]);
        
        replicate.data = NULL;
        free(message_clip);
        message_clip = NULL;
    }
    
    #ifdef DEBUG
    printf("[log] thread: threadID=%lu\n\t- closing connection and exiting.\n\n", pthread_self());
    #endif
    
    // when it receives EOF closes connection
    shutdown(client.fd, SHUT_RDWR);
    close(client.fd);
    
    free(message_clip);
    free(replicate.data);
    
    // remove client from the list of all clients
    pthread_mutex_lock(&mutex[M_USER]);
    update_client_fds(client, RMV_FD);
    pthread_mutex_unlock(&mutex[M_USER]);
    
    // decrement the number of active threads, and if it is zero
    // send a signal to the condition variable so the program can exit safely
    pthread_mutex_lock(&mutex[M_THREAD]);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex[M_DATA_C]);
    pthread_mutex_unlock(&mutex[M_THREAD]);

    //will exit the thread that calls it.
    pthread_exit(NULL);
}

void* timestamp_thread_handler(void *args){
    client_t client = *((client_t *) args);
    pthread_mutex_unlock(&mutex[M_CPY_R]);
    
    char message_timestamp[sizeof(timestamp_t)];
    char message[sizeof(message_t)];
    int return_val;
    timestamp_t ts;
    message_t m1;
    
    client.type = REMOTE_T;
    
    // update the list of clients with the new remote client
    return_val = WAIT;
    while(return_val == WAIT){
        pthread_mutex_lock(&mutex[M_USER]);
        return_val = update_client_fds(client, ADD_FD);
        pthread_mutex_unlock(&mutex[M_USER]);
        
        if(return_val == WAIT){
            pthread_mutex_lock(&mutex[M_WAIT_FD]);
            pthread_cond_wait(&data_cond_wait, &mutex[M_WAIT_FD]);
            pthread_mutex_unlock(&mutex[M_WAIT_FD]);
        }
    }
    
    // increment number of active threads
    pthread_mutex_lock(&mutex[M_THREAD]);
    nr_threads++;
    pthread_mutex_unlock(&mutex[M_THREAD]);
    
    #ifdef DEBUG
    printf("[log] thread: threadID=%lu\n\t- was created to pass timestamp to new remote client: %s\n", pthread_self(), client.sin_addr);
    #endif

    while(recv(client.fd, message, sizeof m1, 0) > 0){
        memcpy(&m1, message, sizeof m1);
        #ifdef DEBUG
        printf("[log] thread: threadID=%lu \n", pthread_self());
        #endif
        
        if(m1.operation == TMSTAMP){
            ts = get_timestamp();
            #ifdef DEBUG
            printf("\t-timestamp sent: %d:%d:%d:%lu\n", ts.tm_struct.tm_hour, ts.tm_struct.tm_min, ts.tm_struct.tm_sec, ts.tv.tv_usec);
            #endif
            memcpy(message_timestamp, &ts, sizeof(timestamp_t));
            if(write(client.fd, message_timestamp, sizeof(timestamp_t)) == -1) //could not send, just ignore it
                perror(E_WRITE);
        }
    }
    
    #ifdef DEBUG
    printf("[log] thread: threadID=%lu\n\t- closing connection and exiting.\n\n", pthread_self());
    #endif

    // when it receives EOF closes connection
    shutdown(client.fd, SHUT_RDWR);
    close(client.fd);

    // remove client from the list of all clients
    pthread_mutex_lock(&mutex[M_USER]);
    update_client_fds(client, RMV_FD);
    pthread_mutex_unlock(&mutex[M_USER]);

    // decrement the number of active threads, and if it is zero
    // send a signal to the condition variable so the program can exit safely
    pthread_mutex_lock(&mutex[M_THREAD]);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex[M_DATA_C]);
    pthread_mutex_unlock(&mutex[M_THREAD]);

    //will exit the thread that calls it.
    pthread_exit(NULL);
}

// thread to handle accept new local clients
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
    client.wait = -1;
    client.wait_size = 0;
    
    // increment number of active threads
    pthread_mutex_lock(&mutex[M_THREAD]);
    nr_threads++;
    if(nr_threads == 3)
        pthread_cond_signal(&data_cond); // signal if all initial threads have been created
    pthread_mutex_unlock(&mutex[M_THREAD]);

    // accept new connection from a new local client
    while((client.fd = accept(l_sock_fd, (struct sockaddr *) &client_addr, &size_addr)) != -1){
        if(client.fd == -1){
            perror(E_ACCEPT);
            continue;
        }
        
        #ifdef DEBUG
        printf("[log] thread: threadID=%lu\n\t- acceped connection from: %s\n\n", pthread_self(), client_addr.sun_path);
        #endif
        
        pthread_mutex_lock(&mutex[M_CPY_L]);
        
        // create a new thread to handle the communication with the new local client
        if(pthread_create(&thread_id, NULL, local_thread_handler, (void *)&client) != 0){
            perror(E_T_CREATE);
            continue;
        }
        
        // detach thread so when it terminates, its resources are automatically released
        if (pthread_detach(thread_id)){
            perror(E_T_DETACH);
            continue;
        }
        
        // ensures that it is locked until client has been copied
        pthread_mutex_lock(&mutex[M_CPY_L]);
        pthread_mutex_unlock(&mutex[M_CPY_L]);
        
    }
    
    printf("[log] thread: threadID=%lu\n\t- exiting.\n\n", pthread_self());
    
    // decrement the number of active threads, and if it is zero
    // send a signal to the condition variable so the program can exit safely
    pthread_mutex_lock(&mutex[M_THREAD]);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex[M_DATA_C]);
    pthread_mutex_unlock(&mutex[M_THREAD]);
    
    pthread_exit(NULL);
}

// thread to handle accept new remote clients
void* accept_remote_client_handler(void *args){
    struct sockaddr_in client_addr;
    socklen_t size_addr = sizeof(struct sockaddr);
    pthread_t thread_id;
    client_t client;
    
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    
    printf("\n[log] thread: threadID=%lu\n\t- was created to handle accept remote clients\n", pthread_self());
    
    client.type = REMOTE_C;
    client.wait = -1;
    client.wait_size = 0;
    
    // increment number of active threads
    pthread_mutex_lock(&mutex[M_THREAD]);
    nr_threads++;
    if(nr_threads == 3)
        pthread_cond_signal(&data_cond); // signal if all initial threads have been created
    pthread_mutex_unlock(&mutex[M_THREAD]);

    // accept new connection from a new remote client
    while((client.fd = accept(r_in_sock_fd, (struct sockaddr *) &client_addr, &size_addr)) != -1){
        if(client.fd == -1){
            perror(E_ACCEPT);
            continue;
        }

        // copy the client's sin_addr
        strcpy(client.sin_addr, inet_ntoa(client_addr.sin_addr));
        
        #ifdef DEBUG
        printf("[log] thread: threadID=%lu\n\t- acceped connection from: %s\n\n", pthread_self(), client.sin_addr);
        #endif
        
        pthread_mutex_lock(&mutex[M_CPY_R]);
        
        // create a new thread to handle the communication with the new remote client
        if(pthread_create(&thread_id, NULL, remote_thread_handler, (void *)&client) != 0){
            perror(E_T_CREATE);
            continue;
        }
        
        // detach thread so when it terminates, its resources are automatically released
        if (pthread_detach(thread_id)){
            perror(E_T_DETACH);
            continue;
        }
        
        // ensures that it is locked until client has been copied
        pthread_mutex_lock(&mutex[M_CPY_R]);
        pthread_mutex_unlock(&mutex[M_CPY_R]);
    }
    
    printf("[log] thread: threadID=%lu\n\t- exiting.\n\n", pthread_self());
    
    
    // decrement the number of active threads, and if it is zero
    // send a signal to the condition variable so the program can exit safely
    pthread_mutex_lock(&mutex[M_THREAD]);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex[M_DATA_C]);
    pthread_mutex_unlock(&mutex[M_THREAD]);
    
    pthread_exit(NULL);
}

void* accept_timestamp_client_handler(void *args){
    struct sockaddr_in client_addr;
    socklen_t size_addr = sizeof(struct sockaddr);
    pthread_t thread_id;
    client_t client;

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;

    printf("\n[log] thread: threadID=%lu\n\t- was created to handle accept top connections\n", pthread_self());

    client.type = REMOTE_T;
    client.wait = -1;
    client.wait_size = 0;

    // increment number of active threads
    pthread_mutex_lock(&mutex[M_THREAD]);
    nr_threads++;
    if(nr_threads == 3)
        pthread_cond_signal(&data_cond); // signal if all initial threads have been created
    pthread_mutex_unlock(&mutex[M_THREAD]);

    // accept new connection from a new remote client
    while((client.fd = accept(t_in_sock_fd, (struct sockaddr *) &client_addr, &size_addr)) != -1){
        if(client.fd == -1){
            perror(E_ACCEPT);
            continue;
        }

        // copy the client's sin_addr
        strcpy(client.sin_addr, inet_ntoa(client_addr.sin_addr));

        #ifdef DEBUG
        printf("[log] thread: threadID=%lu\n\t- acceped top connection from: %s\n\n", pthread_self(), client.sin_addr);
        #endif

        pthread_mutex_lock(&mutex[M_CPY_R]);

        // create a new thread to handle the communication with the new remote client
        if(pthread_create(&thread_id, NULL, timestamp_thread_handler, (void *)&client) != 0){
            perror(E_T_CREATE);
            continue;
        }

        // detach thread so when it terminates, its resources are automatically released
        if (pthread_detach(thread_id)){
            perror(E_T_DETACH);
            continue;
        }

        // ensures that it is locked until client has been copied
        pthread_mutex_lock(&mutex[M_CPY_R]);
        pthread_mutex_unlock(&mutex[M_CPY_R]);
    }

    printf("[log] thread: threadID=%lu\n\t- exiting.\n\n", pthread_self());


    // decrement the number of active threads, and if it is zero
    // send a signal to the condition variable so the program can exit safely
    pthread_mutex_lock(&mutex[M_THREAD]);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex[M_DATA_C]);
    pthread_mutex_unlock(&mutex[M_THREAD]);

    pthread_exit(NULL);
}


// thread to handle the replication of data
void* replicate_copy_cmd(void *args){
    replicate_t replicate = *((replicate_t *) args);
    size_t size = replicate.message.size;
    void* data = replicate.data;
    replicate.data = NULL;
    
    int error = 0;
    if ((replicate.data = (void*) realloc(replicate.data, size)) == NULL){
        perror(E_REALLOC);
        error = 1;
    }
    
    if(!error)
        memcpy(replicate.data, data, size);
    
    pthread_mutex_unlock(&mutex[M_REPL]);
    
    if(error)
        pthread_exit(NULL);

    char message[sizeof(message_t)];
    int i, n_user;
    
    pthread_mutex_lock(&mutex[M_USER]);
    n_user = nr_users;
    pthread_mutex_unlock(&mutex[M_USER]);

    // serialize struct(message_t)
    memcpy(message, &replicate.message, sizeof(message_t));
    
    // increment number of active threads
    pthread_mutex_lock(&mutex[M_THREAD]);
    nr_threads++;
    pthread_mutex_unlock(&mutex[M_THREAD]);

    #ifdef DEBUG
    printf("\n[log] thread: threadID=%lu\n\t- was created to handle replication of region %d\n", pthread_self(), replicate.message.region);
    #endif
    
    client_t *all_client_fd_aux;    
    if ((all_client_fd_aux = (client_t*) malloc(n_user*sizeof(client_t) )) == NULL)
        perror(E_MALLOC);
    else{
        // copy the list with all clients of this clipboard, to avoid lock mutexes when calling write to the socket because its a blocking function
        pthread_mutex_lock(&mutex[M_USER]);    
        memcpy(all_client_fd_aux, all_client_fd, n_user*sizeof(client_t)); 
        pthread_mutex_unlock(&mutex[M_USER]);

        // for all existing clients of this clipboard
        for(i = 0; i < n_user; i++){
            // if it is a clipboard client or a clipboard server
            if(all_client_fd_aux[i].type >= REMOTE_C && all_client_fd_aux[i].type < REMOTE_T){
                
                // if file descriptor is different from the received (the clipboard that have sent data to this one) or if it was a local client
                // that send data, then,
                if((all_client_fd_aux[i].fd != replicate.client.fd && replicate.client.type >= REMOTE_C) || replicate.client.type == LOCAL){
                    
                    pthread_mutex_lock(&mutex[M_REPLI]);
                    
                    // send message info
                    if(write(all_client_fd_aux[i].fd, message, sizeof(message_t)) <= 0) {
                        perror(E_WRITE);
                        shutdown(all_client_fd_aux[i].fd, SHUT_RDWR); // if an error occur, then close the client fd and the resposible thread will receive and EOF that will
                        close(all_client_fd_aux[i].fd);               // be resposible to do the safe remove of this client
                    }
                        
                    // send the actual data
                    if(write(all_client_fd_aux[i].fd, replicate.data, replicate.message.size) <= 0) {
                        perror(E_WRITE);
                        shutdown(all_client_fd_aux[i].fd, SHUT_RDWR);
                        close(all_client_fd_aux[i].fd);
                    }
                    
                    pthread_mutex_unlock(&mutex[M_REPLI]);
                }
                
            // if the client is a local client and it is in the waitng state and it is waiting for this region, then
            }else if(all_client_fd_aux[i].type == LOCAL && all_client_fd_aux[i].wait == replicate.message.region){
                
                // replicate the data to him with re asked size
                if(write(all_client_fd_aux[i].fd, replicate.data, all_client_fd_aux[i].wait_size) <= 0){
                    perror(E_WRITE);
                    shutdown(all_client_fd_aux[i].fd, SHUT_RDWR);
                    close(all_client_fd_aux[i].fd);
                }
                
                #ifdef DEBUG
                printf("\t- sent[l]: region=%d | message=%s\n", replicate.message.region, (char*)replicate.data);
                #endif
                
                all_client_fd_aux[i].wait = -1;
                all_client_fd_aux[i].wait_size = 0;
            }
        }
    }
    
    free(all_client_fd_aux);
    free(replicate.data);
    
    #ifdef DEBUG
    printf("\t- exiting.\n\n");
    #endif
    
    // decrement the number of active threads, and if it is zero
    // send a signal to the condition variable so the program can exit safely
    pthread_mutex_lock(&mutex[M_THREAD]);
    if(!(--nr_threads)) pthread_mutex_unlock(&mutex[M_DATA_C]);
    pthread_mutex_unlock(&mutex[M_THREAD]);
    
    pthread_exit(NULL);
}

void init(){
    char message_timestamp[sizeof(timestamp_t)];
    char message[sizeof(message_t)];
    timestamp_t ts;
    message_t m1;
    int i;
    
    // create an initial vector of all clients of this clipboard
    if((all_client_fd = (client_t*) malloc( INITIAL_NR_FD * sizeof(client_t) )) == NULL){
        perror(E_MALLOC);
        secure_exit(1);
    }
    
    if((top_clip_address = (struct sockaddr_in *) realloc(top_clip_address, sizeof(struct sockaddr_in))) == NULL){
        perror(E_MALLOC);
        secure_exit(1);
    }
    
    if(remote_connection){
        m1.operation = TMSTAMP;
        m1.size = sizeof(timestamp_t);

        memcpy(message, &m1, sizeof m1);

        // sends info message to clipboard
        if(write(top_clip_client.fd, message, sizeof m1) <= 0){
            perror(E_WRITE);
            secure_exit(1);
        }

        if(recv(top_clip_client.fd, message_timestamp, sizeof(timestamp_t), 0) <= 0){
            perror(E_RECV);
            secure_exit(1);
        }
        memcpy(&ts, message_timestamp, sizeof(timestamp_t));
    }else
        ts = get_timestamp();
    
    // init each region of the clipboard and initialize rwlock
    for(i = 0; i < NREGIONS; i++){
        clipboard[i].data = NULL;
        clipboard[i].size = 0;
        clipboard[i].ts = ts;
        
        if(pthread_rwlock_init(&rwlock_clip[i], NULL)){
            perror(E_RWLOCK);
        }
    }
    
    unlink(SOCK_ADDRESS);

    printf("\n***CLIPBOARD***\n\n[log] thread main: threadID:%lu\n", pthread_self());
}

// verify the input arguments
void verifyInputArguments(int argc, char* argv[]){
    pthread_t thread_id;
    client_t remote;
    int port/*, tmp_port*/;
    
    if(argc != 1 && argc != 4)
        inv(I_INPUT_USAGE);

    if(argc == 4){
        // verify if wass received a flag "-c"
        if(memcmp(argv[1], "-c", strlen(argv[1])) != 0)
            inv(I_OPTION);

        // verify if it is a valid ip address
        if(!isValidIpAddress(argv[2]))
            inv(I_IP);
        
        // verify if it is a valid port
        port = atoi(argv[3]);
        if(port < MIN_PORT || port > MAX_PORT)
            inv(I_PORT);

        // connect to the remote backup
        remote = connect_server(argv[2], port, r_out_sock_fd);
        remote_connection = 1;
        
        if(has_top)            
            top_clip_client = connect_server(inet_ntoa(top_clip_address->sin_addr), ntohs(top_clip_address->sin_port), t_out_sock_fd);
        
        pthread_mutex_lock(&mutex[M_CPY_R]);
        
        // create a new thread to handle the communication with the new remote client
        if(pthread_create(&thread_id, NULL, remote_thread_handler, (void *)&remote) != 0){
            perror(E_T_CREATE);
            secure_exit(1);
        }
        // detach thread so when it terminates, its resources are automatically released
        if (pthread_detach(thread_id)){
            perror(E_T_DETACH);
            secure_exit(1);
        }
        
        // ensures that it is locked until copying client
        pthread_mutex_lock(&mutex[M_CPY_R]);
        pthread_mutex_unlock(&mutex[M_CPY_R]);
        
        printf("\n");
    }else{
        top_clip_address->sin_family = AF_INET;
        open_timestamp_socket();
        inet_aton("255.255.255.255", &top_clip_address->sin_addr);
    }
}

void open_local_socket(){
    struct sockaddr_un local_addr;

    // create socket
    if((l_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1){
        perror(E_SOCKET);
        secure_exit(1);
    }

    local_addr.sun_family = AF_UNIX;
    strcpy(local_addr.sun_path, SOCK_ADDRESS);

    // bind socket -> assigns the address to the socket referred to by the file descriptor sockfd
    if(bind(l_sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) == -1){
        perror(E_BIND);
        secure_exit(1);
    }

    // marks the socket as passive socket -> it will be used to accept incoming connection
    if(listen(l_sock_fd, NR_BACKLOG) == -1){
        perror(E_LISTEN);
        secure_exit(1);
    }
    
    printf("\t- local socket created and binded with name = %s\n", SOCK_ADDRESS);
}

void open_remote_socket(){
    struct sockaddr_in remote_addr;

    // create socket
    r_in_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (r_in_sock_fd == -1){
        perror(E_SOCKET);
        secure_exit(1);
    }
    
    // generate a random port
    srand(getpid());
    int range = MAX_PORT - MIN_PORT + 1;
    int port = rand()%range + MIN_PORT;

	memset(&remote_addr, 0, sizeof(struct sockaddr));
    
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port= htons(port);
    remote_addr.sin_addr.s_addr= INADDR_ANY;

    // bind socket -> assigns the address to the socket referred to by the file descriptor sockfd
    if(bind(r_in_sock_fd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) == -1){
        perror(E_BIND);
        secure_exit(1);
    }
    
    // marks the socket as passive socket -> it will be used to accept incoming connection
    if(listen(r_in_sock_fd, NR_BACKLOG) == -1){
        perror(E_LISTEN);
        secure_exit(1);
    }
    
    printf("\t- remote socket created and binded in port = %d\n", ntohs(remote_addr.sin_port));
    top_clip_address->sin_port = htons(ntohs((in_port_t)remote_addr.sin_port) + 1);
}

void open_timestamp_socket(){
    // create socket
    t_in_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (t_in_sock_fd == -1){
        perror(E_SOCKET);
        secure_exit(1);
    }

    // bind socket -> assigns the address to the socket referred to by the file descriptor sockfd
    if(bind(t_in_sock_fd, (struct sockaddr *)top_clip_address, sizeof(struct sockaddr)) == -1){
        perror(E_BIND);
        secure_exit(1);
    }

    // marks the socket as passive socket -> it will be used to accept incoming connection
    if(listen(t_in_sock_fd, NR_BACKLOG) == -1){
        perror(E_LISTEN);
        secure_exit(1);
    }

    printf("\t- top clip socket created and binded in port = %d\n", ntohs(top_clip_address->sin_port));
}

client_t connect_server(char *ipAddress, int port, int sock_fd){
    client_t remote;
    struct sockaddr_in server_addr;
    char buf[sizeof(struct sockaddr_in)];
    
    // create socket to the remote server
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1){
        perror(E_SOCKET);
        secure_exit(1);
    }

	memset(&server_addr, 0, sizeof(struct sockaddr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_aton(ipAddress, &server_addr.sin_addr);

    // connect to this socket
    if(connect(sock_fd, (const struct sockaddr *) &server_addr, sizeof(struct sockaddr)) == -1){
        perror(E_CONN);
        secure_exit(1);
    }
    
    printf("\t- connected with success to %s:%d\n", inet_ntoa(server_addr.sin_addr), port);

    remote.fd = sock_fd;
    remote.type = REMOTE_S;
    memcpy(remote.sin_addr, inet_ntoa(server_addr.sin_addr), sizeof(remote.sin_addr));    
    
    if(!has_top){
        if(recv(remote.fd, buf, sizeof buf, 0) <= 0){
            perror(E_RECV);
            secure_exit(1);
        }
        memcpy(top_clip_address, buf, sizeof buf);
        if(strcmp(inet_ntoa(top_clip_address->sin_addr), "255.255.255.255") == 0)
            inet_aton(ipAddress, &top_clip_address->sin_addr);
        
        has_top = 1;
    }
    
    return remote;
}

timestamp_t get_timestamp(){
    timestamp_t ts;
    time_t time_v = time(NULL);
    gettimeofday(&ts.tv, 0);

    memcpy(&ts.tm_struct,(localtime(&time_v)), sizeof(struct tm));
    return ts;
}

int update_client_fds(client_t client, int operation){
    int i, flag = 0, random;
    
    // add a new client
    if(operation == ADD_FD){
        // allocate more memory if needed
        if(nr_users < MAX_NR_FD) {
            if(++nr_users > INITIAL_NR_FD) {
                if ((all_client_fd = (client_t*) realloc(all_client_fd, nr_users * sizeof(client_t))) == NULL) {
                    nr_users--;
                    p_error(E_REALLOC); // unexpected error, close clipboard
                }
            }
        
            all_client_fd[nr_users - 1] = client;
        }else{
            
            srand(getpid());
            do{
                random = rand()%nr_users;
            }while(all_client_fd[random].type != LOCAL); //testar isto -------------------------------------------------------------------------------------------------------------------------------
            
            #ifdef DEBUG
            printf("random=%d => closing fd = %d\n\n", random, all_client_fd[random].fd);
            #endif
            
            // shut and close a random connection with a local client
            // thread that handles this communication will recv an EOF
            shutdown(all_client_fd[random].fd, SHUT_RDWR);
            close(all_client_fd[random].fd);
            
            return WAIT;
        }
        
    }else{
        
        // sift all clients
        for(i = 0; i < nr_users; i++){
            if(!flag){
                if(all_client_fd[i].fd == client.fd)
                    flag = 1;
            }else{
                all_client_fd[i - 1] = all_client_fd[i];
            }
        }
        
        // remove some memory if needed
        if(flag && --nr_users > INITIAL_NR_FD)
            if ((all_client_fd = (client_t*) realloc(all_client_fd, nr_users * sizeof(client_t))) == NULL)
                p_error(E_REALLOC); // unexpected error, close clipboard
                
        if(nr_users == MAX_NR_FD - 1)
            pthread_cond_signal(&data_cond_wait);
    }
    
    return 1;
}

int compare_timestamp(timestamp_t ts1, timestamp_t ts2){
    int t1 = mktime(&ts1.tm_struct);
    int t2 = mktime(&ts2.tm_struct);
    
    double diffSecs = difftime(t1, t2);
    
    if(diffSecs > 0)
        return 0;
    else if(diffSecs < 0)
        return 1;
    
    diffSecs = ts1.tv.tv_usec - ts2.tv.tv_usec;
    if(diffSecs > 0)
        return 0;
    else
        return 1;
}

int isValidIpAddress(char *ipAddress) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
}

void ctrl_c_callback_handler(){
    printf("[sig] caught signal Ctr-C\n\n");
    pthread_cond_signal(&data_cond);
}

void broken_pipe_callback_handler() {
    struct sigaction act;
    
    printf("[sig] caught signal broken pipe\n\n");
    
    memset(&act, 0, sizeof(act));
    
    act.sa_handler = SIG_IGN;
    act.sa_flags = SA_RESTART;
    
    if(sigaction(SIGPIPE, &act, NULL))
        perror(E_SIG);
}

void p_error(char* msg){
    perror(msg);
    pthread_cond_signal(&data_cond);
}

void inv(char* msg){
    printf(msg);
    secure_exit(1);
}

void secure_exit(int flag){
    int i;
    
    if(!flag)
        pthread_mutex_lock(&mutex[M_DATA_C]);
    
    if (all_client_fd != NULL){
        for(i = 0; i < nr_users; i++){
            shutdown(all_client_fd[i].fd, SHUT_RDWR);
            close(all_client_fd[i].fd);
        }
    }
    
    shutdown(l_sock_fd, SHUT_RDWR);
    close(l_sock_fd);
    shutdown(r_out_sock_fd, SHUT_RDWR);
    close(r_out_sock_fd);
    shutdown(r_in_sock_fd, SHUT_RDWR);
    close(r_in_sock_fd);
    shutdown(t_in_sock_fd, SHUT_RDWR);
    close(t_in_sock_fd);
    shutdown(t_out_sock_fd, SHUT_RDWR);
    close(t_out_sock_fd);
    
    unlink(SOCK_ADDRESS);

    if(!flag){
        // waits until every thread are closed
        pthread_mutex_lock(&mutex[M_DATA_C]);
        pthread_mutex_unlock(&mutex[M_DATA_C]);
    }
    
    free(all_client_fd);
    free(top_clip_address);
    
    for(i = 0; i < NREGIONS; i++){
        pthread_rwlock_destroy(&rwlock_clip[i]);
        free(clipboard[i].data);
    }
    
    for(i = 0; i < NR_MUTEXES; i++)
        pthread_mutex_destroy(&mutex[i]);    
    
    pthread_cond_destroy (&data_cond);
    pthread_cond_destroy (&data_cond_wait);

    printf("\n[!] exiting from clipboard\n\n");
    
    if(flag)
        exit(-1);
}
