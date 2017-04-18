#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int proceed = 1;
void alarm_handler();

void main() {
    unsigned long serial = 0;
    struct sigaction sa;

    alarm(10);

    sa.sa_handler = alarm_handler;
    sigaction(SIGALRM, &sa, NULL);

    while(proceed) {
        serial++;
    }

    printf("The variable serial now equals to %lu.\n", serial);
}

void alarm_handler() {
    proceed = 0;
}
