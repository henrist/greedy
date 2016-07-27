#ifndef COMMON_FILE
#define COMMON_FILE

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>
#include <linux/tcp.h>
#include <pthread.h>
#include <sys/utsname.h>

#define REPORT_EVERY_MS 50
#define BUFF_SIZE 100*1024

static volatile int exit_program = 0;
static volatile int sockfd, total = 0, last = 0;
static char type[8] = "unknown";

int new_tcp = 0;

struct utsname unamedata;

void init() {
    struct utsname unamedata;
    int v1, v2;

    if (uname(&unamedata) == 0 && sscanf(unamedata.release, "%d.%d.", &v1, &v2) == 2) {
        if (v1 > 4 || (v1 == 4 && v2 >= 6)) {
            new_tcp = 1;
        }
    }
}

void int_handler(int dummy) {
    exit_program = 1;
}

void generate_status()
{
    struct tcp_info info;
    int len = sizeof(struct tcp_info);
    getsockopt(sockfd, IPPROTO_TCP, TCP_INFO, &info, &len);

    int in_flight = info.tcpi_unacked - (info.tcpi_sacked + info.tcpi_lost) + info.tcpi_retrans;

    printf("%-5s ", type);
    printf("%6d kb, in_flight=%8d p, lost=%5d, snd_cwnd=%8d, buffer=%8d\n",
        (total-last)/1024,
        in_flight,
        info.tcpi_lost,
        info.tcpi_snd_cwnd,
        new_tcp ? info.tcpi_notsent_bytes : -1);

    last = total;
}

void logging_thread_run()
{
    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = REPORT_EVERY_MS * 1000000;

    while (1) {
        nanosleep(&sleeptime, NULL);
        generate_status();
    }
}

#endif
