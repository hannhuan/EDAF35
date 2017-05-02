#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void main() {
while(1){
    sigset_t block_mask, pending_mask;
    sigemptyset(&block_mask);

    // Block as many signums as possible, including unknown ones.
    // NSIG: the max val. of signals, extracted from signal.h.
 
    for (int signum=1; signum < NSIG; signum++) {
        sigaddset(&block_mask, signum);
    }

    sigprocmask(SIG_BLOCK, &block_mask, NULL);

    printf("Going to bed..\n");

    while(1) {
        sleep(5);
        break;
    }
    
    printf("Woken up T_T\n");

    sigpending(&pending_mask);

    for (int signum=1; signum < NSIG; signum++) {
        if (sigismember(&pending_mask, signum)) {
            printf("Signal num %d blocked during the loop.\n", signum);
        }
    }
}
}
