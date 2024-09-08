/*
 * ianmclinden, 2024
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <unistd.h>

#ifdef DEBUG
#define syslog(b, ...)       \
    {                        \
        printf(__VA_ARGS__); \
        printf("\n");        \
    }
#endif

// Defaults
const char LOG_IDENT[] = "aesdsocket";
const uint16_t DEFAULT_PORT = 9000;
const char DEFAULT_LOGFILE_PATH[] = "/var/tmp/aesdsocketdata";
const int SVR_BACKLOG = 16;
const size_t BUF_BLKSZ = 4096;
const long int LOG_IVAL_SEC = 10;

const struct option longopts[] = {
    {"help", no_argument, NULL, 'h'},
    {"daemonize", no_argument, NULL, 'd'},
    {"port", required_argument, NULL, 'p'},
    {"logfile", required_argument, NULL, 'f'},
    {NULL, 0, NULL, 0}};
const char *optstring = "hdp:f:";

void print_help()
{
    printf("aesdsocket - assignment 5 socket server\n");
    printf("\n");
    printf("Usage: aesdsocket [options]\n");
    printf("\n");
    printf("Options:\n");
    printf(" --help,-h              Print this help and exit\n");
    printf(" --daemonize, -d        Run the server as a daemon\n");
    printf(" --port, -p <PORT>      Bind to port PORT. (Default: %d)\n", DEFAULT_PORT);
    printf(" --logfile, -f <FILE>   Log output to FILE. (Default '%s')\n", DEFAULT_LOGFILE_PATH);
}

// Being lazy and just allocating some globals
bool daemonize = false;
uint16_t port = DEFAULT_PORT;
const char *logfile_path = DEFAULT_LOGFILE_PATH;

// Non-atomic run flag -  we only have 1 living process accessing this
volatile bool running = true;
pthread_mutex_t logfile_mutex;
int logfile = -1;

static void handle_signals(int signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        syslog(LOG_DEBUG, "Caught signal, exiting");
        running = false;
    }
}

void handle_log(__attribute__((unused)) union sigval val)
{
    time_t now;
    struct tm *now_tm;
    size_t expected, wrote = 0;
    char buf[255] = {0};

    now = time(NULL);
    now_tm = localtime(&now);
    if (NULL == now_tm)
    {
        syslog(LOG_ERR, "failed to acquire local time");
        exit(errno);
    }

    if (0 == strftime(buf, sizeof(buf), "timestamp:%a, %d %b %Y %T %z\n", now_tm))
    {
        syslog(LOG_ERR, "failed to format the current timestamp");
        exit(errno);
    }

    if (0 != pthread_mutex_lock(&logfile_mutex))
    {
        syslog(LOG_ERR, "failed to acquire logfile lock");
        return;
    }

    expected = strlen(buf);
    wrote = (size_t)write(logfile, buf, expected);
    if (expected != wrote)
    {
        syslog(LOG_ERR, "only wrote %ld of %ld bytes", wrote, expected);
    }

    if (0 != pthread_mutex_unlock(&logfile_mutex))
    {
        syslog(LOG_ERR, "failed to release logfile lock");
        return;
    }
}

struct cli_data
{
    int sock;
    struct sockaddr_in addr;
    socklen_t addr_len;
    char *buf;
};

void *handle_client(void *params)
{
    struct cli_data *data = (struct cli_data *)params;

    struct pollfd cli_pfd = {.fd = data->sock, .events = POLLIN};
    char cli_addr_str[INET_ADDRSTRLEN] = {0};
    size_t expected, wrote = 0;
    char *buf_wptr = data->buf;
    long unsigned int buf_size = (long unsigned int)BUF_BLKSZ;
    ssize_t rd = 0;

    cli_pfd.fd = data->sock;

    if (NULL == inet_ntop(AF_INET, &data->addr.sin_addr, cli_addr_str, sizeof(cli_addr_str)))
    {
        syslog(LOG_ERR, "unable to parse client address");
        close(data->sock);
        return NULL;
    }

    syslog(LOG_DEBUG, "Accepted connection from %s", cli_addr_str);

    // Start with a fairly large buffer
    data->buf = malloc(BUF_BLKSZ);
    if (NULL == data->buf)
    {
        syslog(LOG_ERR, "failed to allocate space for client buffer");
        exit(errno);
    }
    memset(data->buf, 0, BUF_BLKSZ);
    buf_wptr = data->buf;

    while (running)
    {
        // Lazy doubling reallocation if we don't have enough for a new read
        if ((size_t)(buf_wptr - data->buf) >= BUF_BLKSZ)
        {
            buf_size *= 2; // ignore potential overflow, that's terabytes
            // could realloc here, but then we'd have to zeroize for convenienve
            // and then still memcpy so just use a fresh malloc
            char *new = malloc(buf_size);
            if (NULL == new)
            {
                syslog(LOG_ERR, "failed to reallocate client buffer");
                break;
            }
            memset(new, 0, buf_size);
            memcpy(new, data->buf, (size_t)(buf_wptr - data->buf));
            free(data->buf);
            buf_wptr = new + (buf_wptr - data->buf);
            data->buf = new;
        }

        if (0 >= poll(&cli_pfd, 1, -1)) // No timeout, just let this handle signals
        {
            continue; // Interrupted, etc
        }

        if (0 >= (rd = read(data->sock, buf_wptr, BUF_BLKSZ)))
        {
            break; // Error or EOF
        }
        buf_wptr += rd;

        // Naively assume that if there are chars in the buffer, then the last char
        // will be a valid one to check, and not space, etc. then, we
        // only need to check from the last written ptr
        if (((size_t)(buf_wptr - data->buf) > 0) && ('\n' == *(buf_wptr - 1)))
        {
            if (0 != pthread_mutex_lock(&logfile_mutex))
            {
                syslog(LOG_ERR, "failed to acquire logfile lock");
                break;
            }

            // Actually had a full line, let's log it and send the logfile back to the client
            expected = strlen(data->buf);
            wrote = (size_t)write(logfile, data->buf, expected);
            if (expected != wrote)
            {
                // Not gonna handle this case
                syslog(LOG_ERR, "only wrote %ld of %ld bytes", wrote, expected);
            }

            // Send return file contents
            if (-1 == lseek(logfile, SEEK_SET, 0))
            {
                syslog(LOG_ERR, "failed to seek to beginning of logfile");
                break;
            }
            while (running)
            {
                // Reusing the the first block of the buffer
                memset(data->buf, 0, BUF_BLKSZ);

                // Read & send BUF_BLKSZ at a time, let the kernel fragment as necessary
                if (0 > (rd = read(logfile, data->buf, BUF_BLKSZ)))
                {
                    break; // Err only
                }
                expected = (size_t)rd;
                wrote = (size_t)write(data->sock, data->buf, expected);
                if (expected != wrote)
                {
                    // Not gonna handle this case
                    syslog(LOG_ERR, "only sent back %ld of %ld bytes", wrote, expected);
                    break;
                }
                if (0 == rd)
                {
                    break; // EOF
                }
            }
            if (0 != pthread_mutex_unlock(&logfile_mutex))
            {
                syslog(LOG_ERR, "failed to release logfile lock");
                break;
            }
            break; // Only one line handled per client, done now
        }
    }

    if (NULL != data->buf)
    {
        free(data->buf);
        data->buf = NULL;
    }
    close(data->sock);
    syslog(LOG_DEBUG, "Closed connection from %s", cli_addr_str);
    return NULL;
}

struct cli_thread
{
    pthread_t thread;
    struct cli_data *data;
    LIST_ENTRY(cli_thread)
    entries;
};

LIST_HEAD(cli_threads, cli_thread);

int main(int argc, char **argv)
{
    // Args & arg parsing
    int opt = -1;

    // Locals
    timer_t timer_id;
    struct itimerspec ts = {
        .it_interval = {.tv_sec = LOG_IVAL_SEC, .tv_nsec = 0},
        .it_value = {.tv_sec = LOG_IVAL_SEC, .tv_nsec = 0},
    };
    struct sigevent se = {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value.sival_ptr = &timer_id,
        .sigev_notify_function = handle_log,
        .sigev_notify_attributes = NULL,
    };
    struct sigaction sa = {.sa_handler = handle_signals, .sa_flags = SA_RESTART};
    struct sockaddr_in bind_addr;
    int svr_sock = -1;
    struct pollfd svr_pfd = {.fd = svr_sock, .events = POLLIN};
    struct cli_threads clis = {.lh_first = NULL}; // Same as LIST_INIT;
    struct cli_thread *cli;

    while (-1 != (opt = getopt_long(argc, argv, optstring, longopts, 0)))
    {
        switch (opt)
        {
        case 'h':
            print_help();
            exit(EXIT_SUCCESS);
        case 'd':
            daemonize = true;
            break;
        case 'p':
            port = (uint16_t)atoi(optarg); // Not going to handle errs
            break;
        case 'f':
            logfile_path = optarg;
            break;
        case ':':
            fprintf(stderr, "Option '%c' requires an argument\n", (char)optopt);
            __attribute__((fallthrough));
        default:
            print_help();
            exit(EXIT_FAILURE);
        }
    }

    if (0 >= (svr_sock = socket(AF_INET, SOCK_STREAM, 0)))
    {
        perror("failed to allocate socket");
        exit(errno);
    }
    if (-1 == setsockopt(svr_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(1)))
    {
        perror("failed to set socket options");
        exit(errno);
    }

    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;
    bind_addr.sin_port = htons(port);
    if (-1 == bind(svr_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)))
    {
        perror("failed to bind socket");
        exit(errno);
    }

    if (daemonize)
    {
        switch (fork())
        {
        case -1:
            exit(errno);
        case 0: // Child
            if (-1 == setsid())
            {
                exit(errno);
            }
            switch (fork()) // Double-fork to ensure we can't acquire a tty
            {
            case -1:
                exit(EXIT_FAILURE);
            case 0: // Grandchild
                break;
            default:
                close(svr_sock); // Ignore errors
                exit(EXIT_SUCCESS);
            }
            if (-1 == chdir("/"))
            {
                exit(errno);
            }
            for (int i = 0; i < 2; i++) // Be lazy, just close std fds
            {
                close(i); // Ignore errors
            }
            // Be lazy, we should redirect everything but just ensure no
            // reading from stdin, no writing to stdout, stderr past this point
            break;
        default:             // Parent
            close(svr_sock); // Ignore errors
            exit(EXIT_SUCCESS);
        }
    }

    openlog(LOG_IDENT, 0, LOG_USER);

    if (0 != pthread_mutex_init(&logfile_mutex, NULL))
    {
        syslog(LOG_ERR, "failed to create logfile mutex");
        exit(EXIT_FAILURE);
    }
    // Assume the path exists
    logfile = open(logfile_path, (O_RDWR | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
    if (-1 == logfile)
    {
        syslog(LOG_ERR, "failed to open logfile '%s'", logfile_path);
        exit(errno);
    }

    if (-1 == timer_create(CLOCK_REALTIME, &se, &timer_id) ||
        (-1 == timer_settime(timer_id, 0, &ts, 0)))
    {
        syslog(LOG_ERR, "failed to create and initialize timer");
        exit(errno);
    }

    if (-1 == listen(svr_sock, SVR_BACKLOG))
    {
        syslog(LOG_ERR, "failed to listen to socket");
        exit(errno);
    }

    if ((-1 == sigemptyset(&sa.sa_mask)) ||
        (-1 == sigaction(SIGINT, &sa, NULL)) ||
        (-1 == sigaction(SIGTERM, &sa, NULL)))
    {
        syslog(LOG_ERR, "failed to set up exit signal handler");
        exit(errno);
    }

    svr_pfd.fd = svr_sock;

    while (running)
    {
        int cli_sock = -1;
        struct sockaddr_in cli_addr = {0};
        socklen_t cli_addrlen = 0;

        if (0 >= poll(&svr_pfd, 1, -1)) // No timeout, just let this handle signals
        {
            break; // Interrupted, etc
        }
        if (0 >= (cli_sock = accept(svr_sock, (struct sockaddr *)&cli_addr, &cli_addrlen)))
        {
            continue;
        }

        struct cli_thread *new = malloc(sizeof(struct cli_thread));
        if (NULL == new)
        {
            syslog(LOG_ERR, "failed to allocate space for client thread");
            exit(errno);
        }
        new->data = malloc(sizeof(struct cli_data));
        if (NULL == new->data)
        {
            syslog(LOG_ERR, "failed to allocate space for client thread data");
            exit(errno);
        }
        new->data->addr = cli_addr;
        new->data->addr_len = cli_addrlen;
        new->data->sock = cli_sock;
        new->data->buf = NULL; // sanity

        if (0 != pthread_create(&new->thread, NULL, handle_client, (void *)new->data))
        {
            syslog(LOG_ERR, "failed to spawn client thread");
            exit(errno);
        }
        LIST_INSERT_HEAD(&clis, new, entries);
    }

    // Deallocate client handler list
    cli = LIST_FIRST(&clis);
    while (cli != NULL)
    {
        // Just use a new stack var
        struct cli_thread *next = LIST_NEXT(cli, entries);
        pthread_join(cli->thread, NULL); // ingore errors
        // Cleanup the thread's data
        if (NULL != cli->data)
        {
            if (NULL != cli->data->buf)
            {
                free(cli->data->buf);
            }
            free(cli->data);
        }
        free(cli);
        cli = next;
    }

    // Ignore errors
    remove(logfile_path);
    pthread_mutex_destroy(&logfile_mutex);
    close(svr_sock);
    closelog();
    return EXIT_SUCCESS;
}