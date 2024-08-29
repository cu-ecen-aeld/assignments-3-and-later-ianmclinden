/*
 * ianmclinden, 2024
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <unistd.h>

#ifdef DEBUG
#define syslog(b, ...)       \
    {                        \
        printf(__VA_ARGS__); \
        printf("\n");        \
    }
#endif

const char *const LOG_IDENT = "aesdsocket";
const uint16_t DEFAULT_PORT = 9000;
const char *const DEFAULT_LOGFILE_PATH = "/var/tmp/aesdsocketdata";
const int SVR_BACKLOG = 16;
const size_t BUF_BLKSZ = 4096;

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

// Non-atomic run flag -  we only have 1 living process accessing this
volatile bool running = true;

static void handle_signals(int signo)
{
    if (signo == SIGINT || signo == SIGTERM)
    {
        syslog(LOG_DEBUG, "Caught signal, exiting");
        running = false;
    }
}

int main(int argc, char **argv)
{
    int opt = -1;
    struct sigaction sa = {.sa_handler = handle_signals, .sa_flags = SA_RESTART};
    bool daemonize = false;
    uint16_t port = DEFAULT_PORT;
    const char *logfile_path = DEFAULT_LOGFILE_PATH;
    struct sockaddr_in bind_addr;
    int svr_sock = -1;
    struct pollfd svr_pfd = {.fd = svr_sock, .events = POLLIN};
    int logfile = -1;
    char *buf = NULL;

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

    // Assume the path exists
    logfile = open(logfile_path, (O_RDWR | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
    if (-1 == logfile)
    {
        syslog(LOG_ERR, "failed to listen to open logfile '%s'", logfile_path);
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
        // Lazy variables here, not going to keep track of them
        int cli_sock = -1;
        struct pollfd cli_pfd = {.fd = cli_sock, .events = POLLIN};
        struct sockaddr_in cli_addr;
        socklen_t cli_addrlen = 0;
        char cli_addr_str[INET_ADDRSTRLEN] = {0};
        size_t expected, wrote = 0;
        char *buf_wptr = buf;
        long unsigned int buf_size = (long unsigned int)BUF_BLKSZ;
        ssize_t rd = 0;

        // Start with a fairly large buffer
        buf = malloc(BUF_BLKSZ);
        if (NULL == buf)
        {
            syslog(LOG_ERR, "failed to allocate space for client buffer");
            exit(errno);
        }
        memset(buf, 0, BUF_BLKSZ);
        buf_wptr = buf;

        if (0 >= poll(&svr_pfd, 1, -1)) // No timeout, just let this handle signals
        {
            break; // Interrupted, etc
        }

        if (0 >= (cli_sock = accept(svr_sock, (struct sockaddr *)&cli_addr, &cli_addrlen)))
        {
            continue;
        }
        if (NULL == inet_ntop(AF_INET, &cli_addr.sin_addr, cli_addr_str, sizeof(cli_addr_str)))
        {
            syslog(LOG_ERR, "unable to parse client address");
            close(cli_sock);
            continue;
        }

        cli_pfd.fd = cli_sock;

        syslog(LOG_DEBUG, "Accepted connection from %s", cli_addr_str);

        while (running)
        {
            // Lazy doubling reallocation if we don't have enough for a new read
            if ((size_t)(buf_wptr - buf) >= BUF_BLKSZ)
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
                memcpy(new, buf, (size_t)(buf_wptr - buf));
                free(buf);
                buf_wptr = new + (buf_wptr - buf);
                buf = new;
            }

            if (0 >= poll(&cli_pfd, 1, -1)) // No timeout, just let this handle signals
            {
                continue; // Interrupted, etc
            }

            if (0 >= (rd = read(cli_sock, buf_wptr, BUF_BLKSZ)))
            {
                break; // Error or EOF
            }
            buf_wptr += rd;

            // Naively assume that if there are chars in the buffer, then the last char
            // will be a valid one to check, and not space, etc. then, we
            // only need to check from the last written ptr
            if (((size_t)(buf_wptr - buf) > 0) && ('\n' == *(buf_wptr - 1)))
            {
                // Actually had a full line, let's log it and send the logfile back to the client
                expected = strlen(buf);
                wrote = (size_t)write(logfile, buf, expected);
                if (expected != wrote)
                {
                    // Not gonna handle this case
                    syslog(LOG_ERR, "only wrote %ld of %ld bytes", wrote, expected);
                }

                // Send return file contents
                if (-1 == lseek(logfile, SEEK_SET, 0))
                {
                    syslog(LOG_ERR, "Failed to seek to beginning of logfile");
                    break;
                }
                while (running)
                {
                    // Reusing the the first block of the buffer
                    memset(buf, 0, BUF_BLKSZ);

                    // Read & send BUF_BLKSZ at a time, let the kernel fragment as necessary
                    if (0 > (rd = read(logfile, buf, BUF_BLKSZ)))
                    {
                        break; // Err only
                    }
                    expected = (size_t)rd;
                    wrote = (size_t)write(cli_sock, buf, expected);
                    if (expected != wrote)
                    {
                        // Not gonna handle this case
                        syslog(LOG_ERR, "only sent back %ld of %ld bytes", wrote, expected);
                        break;
                    }
                    if (rd == 0)
                    {
                        break; // EOF
                    }
                }

                break; // Only one line handled per client, done now
            }
        }

        if (NULL != buf)
        {
            free(buf);
            buf = NULL;
        }
        close(cli_sock);
        syslog(LOG_DEBUG, "Closed connection from %s", cli_addr_str);
    }

    if (NULL != buf)
    {
        free(buf);
    }
    remove(logfile_path); // Ignore errors
    close(svr_sock);      // Ignore errors
    closelog();           // Ignore errors
    return EXIT_SUCCESS;
}