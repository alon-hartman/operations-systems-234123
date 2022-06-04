#include "segel.h"
#include "request.h"

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
pthread_cond_t c_waiting;
pthread_cond_t c_running;
pthread_mutex_t m_waiting;
pthread_mutex_t m_running;

// HW3: Parse the new arguments too
void getargs(int *port, int *threads, int *queue_size, int *schedalg, int argc, char *argv[])
{
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
    *threads = atoi(argv[2]);
    *queue_size = atoi(argv[3]);
    *schedalg = atoi(argv[4]);
}

void thread_routine() {
    pthread_mutex_lock(&c_waiting);
        while(queueSize(waiting_requests) == 0) {        // there are no requests
            pthread_cond_wait(&c_waiting, &m_waiting);   // wait for a request
        }
        int fd = queuePop(waiting_requests);
        pthread_cond_signal(&c_waiting);
    pthread_mutex_unlock(&m_waiting);
    
    requestHandle(fd);

    queuePush(running_requests, fd);

};

int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen;
    int threads_num, queue_size, schedalg;
    struct sockaddr_in clientaddr;

    getargs(&port, &threads_num, &queue_size, &schedalg, argc, argv);

    // 
    // HW#: init global variables;
    waiting_requests = queueCreate();
    running_requests = queueCreate();

    pthread_cond_init(&c_waiting, NULL);
    pthread_mutex_init(&m_waiting, NULL);

    pthread_cond_init(&c_running, NULL);
    pthread_mutex_init(&m_running, NULL);

    // HW3: Create some threads...
    pthread_t threads[threads_num];
    for(int i=0; i<threads_num; ++i) {
        if(pthread_create(&threads[i], NULL, &thread_routine, NULL) != 0) {
            unix_error("pthread_create failed");
        }
    }
    
    //

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
        
        pthread_mutex_lock(&m_waiting);
            while(queue_size > queueSize(waiting_requests)) {
                pthread_cond_wait(&c_waiting, &m_waiting);
            }
            queuePush(waiting_requests, connfd);
        pthread_mutex_unlock(&m_waiting);
        
        // 
        // HW3: In general, don't handle the request in the main thread.
        // Save the relevant info in a buffer and have one of the worker threads 
        // do the work. 
        // 

        requestHandle(connfd);

        Close(connfd);
    }

}


    


 
