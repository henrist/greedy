#include "common.h"

int main(int argc, char *argv[])
{
    init();

    memcpy(type, "wrote", 6);
    int lsockfd, portno, clilen;
    char buffer[BUFF_SIZE];
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    pthread_t log_thread;
    int pret;

    signal(SIGINT, int_handler);

    if (argc < 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    lsockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lsockfd < 0) {
        fprintf(stderr, "Error opening socket\n");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(lsockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error binding socket\n");
        exit(1);
    }

    printf("waiting for client to connect\n");

    listen(lsockfd, 5);
    clilen = sizeof(cli_addr);
    sockfd = accept(lsockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (sockfd < 0) {
        fprintf(stderr, "Error accepting socket\n");
        exit(1);
    }

    int one = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *) &one, sizeof(one));

    pret = pthread_create(&log_thread, NULL, (void *) &logging_thread_run, NULL);

    bzero(buffer, BUFF_SIZE);
    do {
        n = write(sockfd, buffer, BUFF_SIZE);
        printf(".");
        if (n > 0) {
            total += n;
        }
    } while (n > 0 && !exit_program);

    printf("finished, a total number of %d kbytes was written\n", total/1024);
    close(sockfd);

    return 0;
}
