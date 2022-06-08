#ifndef __REQUEST_H__
#define __REQUEST_H__

/* student defined structures */
typedef struct {
    pthread_t        thread;
    int              id;
    struct timeval   arrival;
    struct timeval   dispatch;
    int              handled_requests;
    int              handled_stat_requests;
    int              handled_dyn_requests;
} threadStats;

void requestHandle(int fd, threadStats *tstats);

#endif
