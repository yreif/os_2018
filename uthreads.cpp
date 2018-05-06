#include "uthreads.h"
#include <sys/time.h>
#include <setjmp.h>
#include <cstdio>
#include <signal.h>
#include <deque>
#include <iostream>
#include <stdlib.h>

#define SECOND 1000000
#define MAIN_THREAD_ID 0

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
		"rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#endif
enum state {
    READY, RUNNING, BLOCKED
};
std::deque<int> ready;
std::deque<int> blocked;
int current_thread;
int total_quantums;

struct itimerval timer;
sigjmp_buf env[MAX_THREAD_NUM];


typedef struct unique_id {

public:

    static std::set<int> s_unUsedID;

    static int generateID() // Generate a valid ID
    {
        static int id = 0;

        if (s_unUsedID.empty()) {
            return -1;
        }
        id = *s_unUsedID.begin();
        s_unUsedID.erase(id);

        return id;
    }

    static void start_me()
    {
        for (int i = 1; i < MAX_THREAD_NUM + 1; ++i) {
            s_unUsedID.insert(i);
        }
    }

    static void removeID(int id) // Remove a given ID from the set of used ID
    {
        s_unUsedID.insert(id);
    }

    static int isIDUsed(int id)
    {
        if (id > MAX_THREAD_NUM)
        {
            return -1;
        }
        return s_unUsedID.count(id) == 0 ? 0:1;
    }

} unique_id;

#define SELF_SWITCH -30
#define TERMINATE_SWITCH -40
#define NO_THREAD -10

/** uthread class */
typedef class uthread uthread;
uthread* uthreads[MAX_THREAD_NUM];

class uthread {
public:
    int quantums;
    std::deque<int> syncedWith;     // thread ids waiting for this thread to run // UPDATE THIS
    int waiting_for;                   // current thread is waiting for synced_t to run // UPDATE THIS
    bool blocked;
    char *stack;
    thread() {
        quatums = 0;
        blocked = false;
        waiting_for = NO_THREAD;
    }
    ~thread(){
        delete[] stack;
    }
};



void switchThreads(int status)
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
        printf("sigaction error.");
    }
    uthreads[current_thread]->quantums += 1;
    total_quantums += 1;

    int ret_val = sigsetjmp(env[current_thread],1);
    if (ret_val == 1) {
        sa.sa_handler = &switchThreads;
        if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
            printf("sigaction error.");
        }
        // initialize timer
        if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
            printf("setitimer error.");
        }
        return;
    } // otherwise, thread not preempted:
    // insert back into ready queue
    if (status == SIGVTALRM) {
        ready.push_back(runningIndex);
    }
    // update threads which were synced with running thread // LO MEVINIM
    if (status != TERMINATE_SWITCH){
        for (int i : uthreads[current_thread]->syncedWith) {
            uthreads[i]->synced_t = NO_THREAD;
            uthreads[current_thread]->syncedWith.pop_front();
            if (!uthreads[i]->blocked){
                ready.push_back(i);
            }
        }
    }
    // remove those synced threads from blocked state
    int j = 0;
    for (int i : blocked)
    {
        if (!uthreads[i]->blocked and uthreads[i]->synced_t == NO_THREAD){
            blocked.erase(blocked.begin()+ j);
        }
        j++;
    }
    if (status == TERMINATE_SWITCH){
        delete uthreads[runningIndex];
        uthreads[runningIndex] = nullptr;
    }

    // update running state
    current_thread = ready.front();
    ready.pop_front();

    set.sa_handler = &switchThreads;
    sigaction(SIGVTALRM, &set, NULL);
    if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
        std::cerr << "thread library error: setitimer error.\n" << std::endl;
        // no error thrown, program will continue running with timing error
        // in order to not disrupt flow
    }

    siglongjmp(env[currentThread],1);
}




void system_error(char *text) {
    fprintf(std::stderr, "system error: %s\n", text);
    free_uthreads();
    exit(1);
}

void uthreads_error(char *text) {
    fprintf(std::stderr, "thread library error: %s\n", text);
}






/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered an error. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid) {
    uthread *to_terminate;
    if (tid == MAIN_THREAD_ID) {
        free_uthreads();
        exit(0);
    }
    if (get_thread(tid, to_terminate) < 0) { // MISSING IMPLEMENTATION
        uthreads_error("no thread with ID tid exists");
        return -1;
    }
    // how to know if a thread terminates itself???
    to_terminate.terminate();
}

/*
 * Description: Releases the assigned library memory.
*/
int free_uthreads() {

}