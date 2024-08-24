/*
 * ianmclinden, 2024
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#define LOG_IDENT "writer"

int main(int argc, char **argv)
{
    // let's be lazy and let exit close the file / closelog (incl error cases)
    openlog(LOG_IDENT, 0, LOG_USER);

    if (argc <= 2)
    {
        syslog(LOG_ERR, "Write file or write string not specified");
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    const char *writestr = argv[2];
    FILE *wptr = fopen(argv[1], "w");
    if (NULL == wptr)
    {
        syslog(LOG_ERR, "Could not open file '%s' for writing", filename);
        return EXIT_FAILURE;
    }

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, filename);

    const size_t expected = strlen(writestr);
    const size_t wrote = fprintf(wptr, "%s", writestr);
    if (expected != wrote)
    {
        syslog(LOG_ERR, "Only wrote %lu of %lu bytes", wrote, expected);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}