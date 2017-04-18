#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

void signal_handler(int);

int main(){
    signal(SIGINT, signal_handler);
   
    while(1){
        printf("I'm a loop...");
        fflush(stdout);
        sleep(2);
    }
    
    return(0);
}

void signal_handler(int signum){
    printf("Caught singal %d", signum);
}
