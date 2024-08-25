#include "systemcalls.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
 */
bool do_system(const char *cmd)
{
    int rc = system(cmd);
    return 0 == rc && 0 == WEXITSTATUS(rc);
}

/**
 * @param count -The numbers of variables passed to the function. The variables are command to execute.
 *   followed by arguments to pass to the command
 *   Since exec() does not perform path expansion, the command to execute needs
 *   to be an absolute path.
 * @param ... - A list of 1 or more arguments after the @param count argument.
 *   The first is always the full path to the command to execute with execv()
 *   The remaining arguments are a list of arguments to pass to the command in execv()
 * @return true if the command @param ... with arguments @param arguments were executed successfully
 *   using the execv() call, false if an error occurred, either in invocation of the
 *   fork, waitpid, or execv() command, or if a non-zero return value was returned
 *   by the command issued in @param arguments with the specified arguments.
 */

#define BS(v) (v > 0 ? "true" : "false")

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    va_end(args);
    command[count] = NULL;

    pid_t cpid, ppid;
    int status;

    cpid = fork();
    switch (cpid)
    {
    case -1:
        return false;
    case 0:
        execv(command[0], command);
        exit(errno);
    default:
        ppid = waitpid(cpid, &status, 0);
        return ((ppid != -1) &&
                !WIFSIGNALED(status) &&
                !WIFSTOPPED(status) &&
                (WIFEXITED(status) && 0 == WEXITSTATUS(status)));
    }
}

/**
 * @param outputfile - The full path to the file to write with command output.
 *   This file will be closed at completion of the function call.
 * All other parameters, see do_exec above
 */
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    va_end(args);
    command[count] = NULL;

    pid_t cpid, ppid;
    int fd;
    int status;

    cpid = fork();
    switch (cpid)
    {
    case -1:
        return false;
    case 0:
        fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0)
        {
            exit(EXIT_FAILURE);
        }
        close(fd);
        execv(command[0], command);
        exit(errno);
    default:
        ppid = waitpid(cpid, &status, 0);
        return ((ppid != -1) &&
                !WIFSIGNALED(status) &&
                !WIFSTOPPED(status) &&
                (WIFEXITED(status) && 0 == WEXITSTATUS(status)));
    }
}
