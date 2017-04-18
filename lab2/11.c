#include <stdio.h>
#include <signal.h>

void sigint_handler(int);
void sigusr2_handler(int);

int main(){
    struct sigaction sa_int;

    sa_int.sa_handler = sigint_handler;

    sigaction(SIGINT, &sa_int, NULL);

    while(1){}
}

void sigint_handler(int signum){
    struct sigaction sa_usr1;
    struct sigaction sa_usr2;

    sa_usr1.sa_handler = SIG_IGN;
    sa_usr2.sa_handler = sigusr2_handler;

    sigaction(SIGUSR1, &sa_usr1, NULL);
    sigaction(SIGUSR2, &sa_usr2, NULL);

    while(1){}
}

void sigusr2_handler(int signum){
    printf("SIGUSR2 caught inside of the SIGINT handler.\n");
    fflush(stdout);
}
