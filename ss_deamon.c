#include "include/ss_core.h"
#include <signal.h>

ss_int_t
ss_daemonize()
{
    ss_int_t    fd, fd1, fd2;
    pid_t       pid;
    struct  sigaction sa;
    
    umask(0);

    if ((pid = fork()) < 0) {
        return -1;
    } else if (pid > 0) {
        exit(0);//parent quit
    }
    //child
    setsid();

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGHUP, &sa, NULL) < 0) {
        return -1;
    }

    fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        return -1;
    }
    if (dup2(fd, STDIN_FILENO) == -1) {
        return -1;
    }
    if (dup2(fd, STDOUT_FILENO) == -1) {
        return -1;
    }
    if (dup2(fd, STDERR_FILENO) == -1) {
        return -1;
    }

    return 0;
}
