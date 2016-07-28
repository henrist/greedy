#include <linux/tcp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_REPORT_MS 100
#define DEFAULT_BUF_SIZE 4096

char *buffer;
int buffer_size = DEFAULT_BUF_SIZE;
int exit_program = 0;
char *hostname;
int keep_running = 0;
int listen_sockfd = -1;
int log_running = 0;
pthread_t log_thread;
enum { MODE_CLIENT, MODE_SERVER } mode = MODE_CLIENT;
int portno;
int report_ms = DEFAULT_REPORT_MS;
int tcp_notsent_capability = 0;
long long total_bytes = 0;
long long total_bytes_buf = 0;
int verbose = 0;

struct bytes_report {
    float val;
    char suffix[4];
    char repr[50];
};

void logging_thread_run(void *arg);

void int_handler(int dummy) {
    exit_program = 1;

    if (listen_sockfd != -1) {
        close(listen_sockfd);
        listen_sockfd = -1;
    }
}

void print_usage(char *argv[]) {
    fprintf(stderr,
        "Usage client: %s <host> <port>\n"
        "Usage server: %s -s <port>\n"
        "Options:\n"
        "  -b n  buffer size in bytes to read/write call (default: %d)\n"
        "  -r    keep server running when client disconnect\n"
        "  -t n  report every n milliseconds, implies -vv (default: %d)\n"
        "  -v    verbose output (more verbose if multiple -v)\n",
        argv[0],
        argv[0],
        DEFAULT_BUF_SIZE,
        DEFAULT_REPORT_MS);
}

void parse_arg(int argc, char *argv[]) {
    int opt;

    while ((opt = getopt(argc, argv, "b:rst:v")) != -1) {
        switch (opt) {
            case 'b':
                buffer_size = atoi(optarg);
                break;
            case 'r':
                keep_running = 1;
                break;
            case 's':
                mode = MODE_SERVER;
                break;
            case 't':
                report_ms = atoi(optarg);
                if (verbose < 2) {
                    verbose = 2;
                }
                break;
            case 'v':
                verbose += 1;
                break;
            default:
                print_usage(argv);
                exit(1);
        }
    }

    if (argc - optind < (mode == MODE_SERVER ? 1 : 2)) {
        print_usage(argv);
        exit(1);
    }

    if (mode == MODE_SERVER) {
        portno = atoi(argv[optind]);
    } else {
        hostname = malloc(strlen(argv[optind]));
        memcpy(hostname, argv[optind], strlen(argv[optind]));

        portno = atoi(argv[optind+1]);
    }
}

void start_logger(int sockfd) {
    int pret;
    pret = pthread_create(&log_thread, NULL, (void *) &logging_thread_run, &sockfd);
    if (pret != 0) {
        fprintf(stderr, "Could not create logging thread\n");
    } else {
        log_running = 1;
    }
}

void stop_logger() {
    if (log_running) {
        pthread_cancel(log_thread);
    }
}

void set_tcp_nodelay(int sockfd) {
    int enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *) &enable, sizeof(enable)) < 0) {
        fprintf(stderr, "setsockopt(TCP_NODELAY) failed");
        exit(1);
    }
}

void get_bytes_format(long long value, struct bytes_report *br, int align) {
    char fmt[20];
    br->val = value;

    if (br->val > 1024) {
        if (br->val > 1024) {
            br->val /= 1024;
            strcpy(br->suffix, "KiB");
        }

        if (br->val > 1024) {
            br->val /= 1024;
            strcpy(br->suffix, "MiB");
        }

        if (br->val > 1024) {
            br->val /= 1024;
            strcpy(br->suffix, "GiB");
        }

        if (align > 0) {
            sprintf(fmt, "%%%d.3f %%s", align-4);
        } else {
            sprintf(fmt, "%%.3f %%s");
        }

        sprintf(br->repr, fmt, br->val, br->suffix);
    }

    else {
        strcpy(br->suffix, "B");

        if (align > 0) {
            sprintf(fmt, "%%%d.0f       %%s", align-8);
        } else {
            sprintf(fmt, "%%.0f %%s");
        }

        sprintf(br->repr, fmt, br->val, br->suffix);
    }
}

void report_closed() {
    if (total_bytes > 0) {
        struct bytes_report br;
        get_bytes_format(total_bytes, &br, 0);

        printf("finished, a total number of %s was %s, %.2f %% of %s buffer used\n",
            br.repr,
            mode == MODE_SERVER ? "written" : "read",
            (float) total_bytes / (float) total_bytes_buf * 100,
            mode == MODE_SERVER ? "write" : "read");
    }
}

void run_client() {
    int read_bytes;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int sockfd;

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "No such host %s\n", hostname);
        exit(1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Error opening socket\n");
        exit(1);
    }

    set_tcp_nodelay(sockfd);

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error connecting to server\n");
        exit(1);
    }

    if (verbose >= 2) {
        start_logger(sockfd);
    }

    //bzero(buffer, buffer_size);
    do {
        read_bytes = read(sockfd, buffer, buffer_size);

        if (read_bytes > 0) {
            if (verbose >= 3) {
                printf(".");
            }
            total_bytes += read_bytes;
            total_bytes_buf += buffer_size;
        } else if (verbose >= 3) {
            printf("x");
        }
    } while (read_bytes > 0 && !exit_program);

    if (verbose) {
        report_closed();
    }

    close(sockfd);
}

void run_server() {
    struct sockaddr_in cli_addr;
    int clilen;
    struct sockaddr_in serv_addr;
    int sockfd;
    int wrote_bytes;

    listen_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sockfd < 0) {
        fprintf(stderr, "Error opening socket\n");
        exit(1);
    }

    int enable = 1;
    if (setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(listen_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error binding socket\n");
        exit(1);
    }

    listen(listen_sockfd, 5);
    clilen = sizeof(cli_addr);

    do {
        if (verbose) {
            printf("waiting for client to connect\n");
        }

        sockfd = accept(listen_sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (exit_program) {
            return;
        }
        if (sockfd < 0) {
            fprintf(stderr, "Error accepting socket\n");
            exit(1);
        }

        set_tcp_nodelay(sockfd);

        if (verbose >= 2) {
            start_logger(sockfd);
        }

        //bzero(buffer, buffer_size);
        do {
            wrote_bytes = write(sockfd, buffer, buffer_size);

            if (wrote_bytes > 0) {
                if (verbose >= 3) {
                    printf(".");
                }
                total_bytes += wrote_bytes;
                total_bytes_buf += buffer_size;
            } else if (verbose >= 3) {
                printf("x");
            }
        } while (wrote_bytes > 0 && !exit_program);

        if (verbose) {
            report_closed();
        }

        stop_logger();
        close(sockfd);
    } while (keep_running && !exit_program);

    if (listen_sockfd != -1) {
        close(listen_sockfd);
    }
}

void detect_tcp_notsent_capability() {
    struct utsname unamedata;
    int v1, v2;

    // tcp_info.tcpi_notsent_bytes is available since Linux 4.6
    if (uname(&unamedata) == 0 && sscanf(unamedata.release, "%d.%d.", &v1, &v2) == 2) {
        if (v1 > 4 || (v1 == 4 && v2 >= 6)) {
            tcp_notsent_capability = 1;
        }
    }
}

int main(int argc, char *argv[])
{
    detect_tcp_notsent_capability();
    signal(SIGINT, int_handler);
    signal(SIGPIPE, SIG_IGN);
    parse_arg(argc, argv);

    buffer = malloc(buffer_size);
    if (buffer == NULL) {
        fprintf(stderr, "Could not allocate memory for buffer (%d bytes)\n", buffer_size);
        exit(1);
    }

    if (mode == MODE_SERVER) {
        run_server();
    } else {
        run_client();
    }

    free(buffer);
    return 0;
}

void logging_thread_run(void *arg)
{
    int sockfd = *((int *) arg);
    long long last = 0;
    struct timespec sleeptime;
    struct bytes_report br;

    sleeptime.tv_sec = report_ms / 1000;
    sleeptime.tv_nsec = (report_ms % 1000) * 1000000;

    while (1) {
        nanosleep(&sleeptime, NULL);

        struct tcp_info info;
        int len = sizeof(struct tcp_info);
        getsockopt(sockfd, IPPROTO_TCP, TCP_INFO, &info, &len);

        int in_flight = info.tcpi_unacked - (info.tcpi_sacked + info.tcpi_lost) + info.tcpi_retrans;

        get_bytes_format(total_bytes-last, &br, 12);

        printf("%-5s %s, in_flight=%8d p, lost=%5d, snd_cwnd=%8d, notsent_bytes=%8d\n",
            mode == MODE_SERVER ? "wrote" : "read",
            br.repr,
            in_flight,
            info.tcpi_lost,
            info.tcpi_snd_cwnd,
            tcp_notsent_capability ? info.tcpi_notsent_bytes : -1);

        last = total_bytes;
    }
}
