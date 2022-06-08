#include "segel.h"
#include "request.h"
#include <assert.h>
// 
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

Queue *waiting_requests;
Queue *running_requests;
pthread_cond_t c = PTHREAD_COND_INITIALIZER;
// pthread_cond_t c_running;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t m_running;

typedef enum {
    BLOCK,
    DROP_TAIL,
    DROP_RAND,
    DROP_HEAD
} OverloadType;

// HW3: Parse the new arguments too
void getargs(int *port, int *threads, int *queue_size, OverloadType *schedalg, int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    char* ot = argv[4];
    if(strcmp(ot, "block") == 0)
        *schedalg = BLOCK;
    else if(strcmp(ot, "dt") == 0)
        *schedalg = DROP_TAIL;
    else if(strcmp(ot, "dh") == 0)
        *schedalg = DROP_HEAD;
    else if(strcmp(ot, "random") == 0)
        *schedalg = DROP_RAND;
    else 
        fprintf(stderr, "Overload Type must be: block|dt|dh|random\n");
}

void* thread_routine(void *arg) {
    threadStats *tstats = (threadStats *)arg;
    tstats->thread = pthread_self();
    printf("\n===%ld was firstly summoned ===\n" ,pthread_self());
    while(1) {
        pthread_mutex_lock(&m);
            while(queueSize(waiting_requests) == 0) {        // there are no requests
                printf("\n===%ld going to sleep ===\n" ,pthread_self());
                pthread_cond_wait(&c, &m);                  // wait for a request
                    printf("\n===%ld im WOKEGE ===\n" ,pthread_self());

            }
            struct timeval arrival;
            int fd = queuePop(waiting_requests, &arrival);
            struct timeval dispatch = {0, 0};
            gettimeofday(&dispatch, NULL);
            tstats->arrival = arrival;
            tstats->dispatch.tv_sec = (dispatch.tv_sec - arrival.tv_sec);
            tstats->dispatch.tv_usec = (dispatch.tv_usec - arrival.tv_usec);
            
            queuePush(running_requests, fd, arrival);
            pthread_cond_broadcast(&c);
        pthread_mutex_unlock(&m);
        
        requestHandle(fd,tstats);
        Close(fd);

        pthread_mutex_lock(&m);
            queuePop(running_requests, NULL);
            pthread_cond_broadcast(&c);
        pthread_mutex_unlock(&m);
    }
    return NULL;
};

int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen;
    int threads_num, queue_size;
    OverloadType schedalg;
    struct sockaddr_in clientaddr;

    getargs(&port, &threads_num, &queue_size, &schedalg, argc, argv);

    // 
    // HW#: init global variables;
    waiting_requests = queueCreate();
    running_requests = queueCreate();

    pthread_cond_init(&c, NULL);
    pthread_mutex_init(&m, NULL);

    // pthread_cond_init(&c_running, NULL);
    // pthread_mutex_init(&m_running, NULL);

    // HW3: Create some threads...
    threadStats threads[threads_num];
    for(int i=0; i<threads_num; ++i) {
        threads[i].id = i;
        threads[i].handled_requests = 0;
        threads[i].handled_stat_requests = 0;
        threads[i].handled_dyn_requests = 0;
        if(pthread_create(&threads[i].thread, NULL, &thread_routine, &threads[i]) != 0) {
            unix_error("pthread_create failed");
        }
    }
    
    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        struct timeval t = {0, 0};
        gettimeofday(&t, NULL);
        
        pthread_mutex_lock(&m);
            printf("--- start ---\n");
            printf("waiting requests print:\n");
            queuePrint(waiting_requests);
            printf("\nrunning requests print:\n");
            queuePrint(running_requests);
            switch(schedalg) {
                case BLOCK:
                    while(queue_size <= queueSize(waiting_requests) + queueSize(running_requests)) {
                        printf("\n~~~MAIN goin to sleep ~~~\n");
                        pthread_cond_wait(&c, &m);
                        printf("\n~~~MAIN woken up ~~~\n");
                    }
                    queuePush(waiting_requests, connfd, t);
                    pthread_cond_signal(&c);
                    break;
                case DROP_TAIL:
                    if(queue_size <= queueSize(waiting_requests) + queueSize(running_requests)) {
                        printf("going to drop %d\n", connfd);
                        Close(connfd);
                    }
                    else {
                        queuePush(waiting_requests, connfd, t);
                        pthread_cond_signal(&c);
                    }
                    break;
                case DROP_RAND:
                    if(queue_size <= queueSize(waiting_requests) + queueSize(running_requests)) {
                        int num = ceil((queueSize(waiting_requests) + queueSize(running_requests)*0.3f));
                        queueDiscardX(waiting_requests,num);  // queue is full
                    }
                    if(queue_size > queueSize(waiting_requests) + queueSize(running_requests)) {
                        queuePush(waiting_requests, connfd, t);  // queue is not full
                        pthread_cond_signal(&c);
                    }
                    else {
                        Close(connfd);
                    }
                    break;
                case DROP_HEAD:
                    if(queue_size <= queueSize(waiting_requests) + queueSize(running_requests)) {
                        int head = queuePop(waiting_requests, NULL);
                        if(head != -1) { 
                            Close(head);
                        }
                        else {
                            Close(connfd);
                        }
                    }
                    else {
                        queuePush(waiting_requests, connfd, t);
                        pthread_cond_signal(&c);
                    }
                    break;
            }
            printf("\n--- end ---\n");
            printf("waiting requests print:\n");
            queuePrint(waiting_requests);
            printf("\nrunning requests print:\n");
            queuePrint(running_requests);
            printf("\n");
        pthread_mutex_unlock(&m);
        

        // 
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads 
        // do the work. 
        // 

        // requestHandle(connfd);

        // Close(connfd);
    }

    //should not be reached
    printf("should never be printed COPIUM\n");
    for (int i = 0; i < threads_num; i++)
    {
        pthread_cancel(threads[i].thread);
    }
    
    assert(pthread_cond_destroy(&c) == 0);
    assert(pthread_mutex_destroy(&m) == 0);

}


    


 
