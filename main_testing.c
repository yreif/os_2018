

#include <stdio.h>
#include <unistd.h>
#include "uthreads.h"

#define SECOND 1000000
#define STACK_SIZE 4096


int first_thread;
int second_thread;

void f(void)
{
    int i = 0;
    while(1){
        ++i;
        printf("in f (%d)\n",i);
        if (i % 3 == 0) {
            printf("f: switching\n");

            uthread_block(first_thread);
        }
        usleep(SECOND);
    }
}

void g(void)
{
    int i = 0;
    while(1){
        ++i;
        printf("in g (%d)\n",i);
        if (i % 5 == 0) {
            printf("g: switching\n");
            uthread_sync(first_thread);
        }
        usleep(SECOND);
    }
}


int main(void)
{
    int suc  = uthread_init(3);
    first_thread = uthread_spawn(&f);
    second_thread = uthread_spawn(&g);
    return 0;
}
