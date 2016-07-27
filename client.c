#include "common.h"

int main(int argc, char *argv[])
{
    init();

    memcpy(type, "read", 5);
    int portno, n;

    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[BUFF_SIZE];

    pthread_t log_thread;
    int pret;

    signal(SIGINT, int_handler);

    if (argc < 3) {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        fprintf(stderr, "Error opening socket\n");
        exit(1);
    }

    int one = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *) &one, sizeof(one));

    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "No such host %s\n", argv[1]);
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error connecting to %s\n", argv[1]);
        exit(1);
    }

    pret = pthread_create(&log_thread, NULL, (void *) &logging_thread_run, NULL);

    bzero(buffer, BUFF_SIZE);
    do {
        n = read(sockfd, buffer, BUFF_SIZE);
        if (n > 0) {
            total += n;
        }
    } while (n > 0 && !exit_program);

    printf("finished, a total number of %d kbytes was read\n", total/1024);
    close(sockfd);

    return 0;
}
