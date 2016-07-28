#include "common.h"

struct hostent *server;
int portno;

void print_usage(char *argv[]) {
    fprintf(stderr, "Usage: %s [-v] <hostname> <port>\nMultiple -v increases verbosity\n", argv[0]);
}

void parse_arg(int argc, char *argv[]) {
    int opt;

    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v':
                verbose += 1;
                break;
            default:
                print_usage(argv);
                exit(1);
        }
    }

    if (argc - optind < 2) {
        print_usage(argv);
        exit(1);
    }

    server = gethostbyname(argv[optind]);
    if (server == NULL) {
        fprintf(stderr, "No such host %s\n", argv[optind]);
        exit(1);
    }

    portno = atoi(argv[optind+1]);
}

int main(int argc, char *argv[])
{
    int n;
    struct sockaddr_in serv_addr;
    char buffer[BUFF_SIZE];

    pthread_t log_thread;
    int pret;

    memcpy(type, "read", 5);
    init();
    parse_arg(argc, argv);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error opening socket\n");
        exit(1);
    }

    int one = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *) &one, sizeof(one));

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error connecting to server\n");
        exit(1);
    }

    if (verbose >= 2) {
        pret = pthread_create(&log_thread, NULL, (void *) &logging_thread_run, NULL);
    }

    bzero(buffer, BUFF_SIZE);
    do {
        n = read(sockfd, buffer, BUFF_SIZE);
        if (n > 0) {
            total += n;
        }
    } while (n > 0 && !exit_program);

    if (verbose) {
        printf("finished, a total number of %d kbytes was read\n", total/1024);
    }

    close(sockfd);

    return 0;
}
