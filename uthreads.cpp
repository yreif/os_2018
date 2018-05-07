#include "uthreads.h"
#include <sys/time.h>
#include <setjmp.h>
#include <cstdio>
#include <signal.h>
#include <deque>
#include <set>
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
private:
    static int max_id = 0;
    static std::set<int> s_unUsedID;
public:
    unique_id(int _max_id) : max_id(_max_id) {
        std::set<int>::iterator iter = s_unUsedID.end();
        for (int i=0; i < max_id; ++i) {
            iter = s_unUsedID.insert(iter, i);
        }
    }

    static int generate_id() // Generate a valid ID
    {
        static int id = 0;

        if (s_unUsedID.empty()) {
            return -1;
        }
        id = *s_unUsedID.begin();
        s_unUsedID.erase(id);

        return id;
    }

    static void remove_id(int id) // Remove a given ID from the set of used ID
    {
        s_unUsedID.insert(id);
    }

    /** returns 0 if in use and 1 otherwise*/
    static int is_id_used(int id)
    {

        return s_unUsedID.count(id) == 0 ? 1:0;
    }

    /** returns 0 if id is valid for library operation and 1 otherwise*/
    static int is_id_valid(int id)
    {
        return (id < max_id and id > 0 and is_id_used(id) == 0) ? 0:1;
    }

} unique_id;

unique_id tids = unique_id(MAX_THREAD_NUM);
#define SELF_SWITCH -30
#define TERMINATE_SWITCH -40
#define RUNNING_BLOCKED_SWITCH -50
#define NO_THREAD -10

/** uthread class */
typedef class thread thread;
thread* uthreads[MAX_THREAD_NUM];
struct sigaction sa;

class thread {
public:
    int quantums;
    std::deque<int> sync_dependencies;     // thread ids waiting for this thread to run // UPDATE THIS
    int waiting_for;                   // current thread is waiting for synced_t to run // UPDATE THIS
    bool blocked;
    char *stack;
    thread() {
        quantums = 0;
        blocked = false;
        waiting_for = NO_THREAD;
    }
    ~thread(){
        delete[] stack;
    }
};

/*
 * Description: Releases the assigned library memory.
*/
int free_uthreads() {

}


void switchThreads(int status) {
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
        /** initialize timer*/
        if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
            printf("setitimer error.");
        }
        return;
    } /** otherwise, thread not preempted:
    ** insert back into ready queue*/
    if (status == SIGVTALRM) {
        ready.push_back(current_thread);
    }

    /**When a running thread blocked itself */
    if (status == RUNNING_BLOCKED_SWITCH){

    }
    /** update threads which were synced with running thread // LO MEVINIM*/
    if (status != TERMINATE_SWITCH){
        for (int i : uthreads[current_thread]->syncedWith) {
            uthreads[i]->synced_t = NO_THREAD;
            uthreads[current_thread]->syncedWith.pop_front();
            if (!uthreads[i]->blocked){
                ready.push_back(i);
            }
        }
    }
    /** remove those synced threads from blocked state*/
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

    sa.sa_handler = &switchThreads;
    sigaction(SIGVTALRM, &set, NULL);
    if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
        std::cerr << "thread library error: setitimer error.\n" << std::endl;
        // no error thrown, program will continue running with timing error
        // in order to not disrupt flow
    }

    siglongjmp(env[currentThread],1);
}


/**
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
 * @param f = function f with the signature void f(void)
 * @return the ID of the created thread.
 * On failure, return -1.
 */
int uthread_spawn(void (*f)(void)){
    int curr_id = unique_id.generateID();
    if(curr_id == -1){
        return curr_id;
    }
    uthread curr_thread = thread();
    try {
        curr_thread.*stack = new char[STACK_SIZE];
    } catch (){
        ~curr_thread();
        return -1;
    }
    uthreads[curr_id] = curr_thread; // maybe & ??
    ready.pushback(curr_id);

    return curr_id;
}

/**
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
    sigemptyset(&sa);
    sigaddset(&sa, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &sa, NULL);

    if (!unique_id.is_id_valid(tid)) {
        std::cerr << "thread library error: terminate called with invalid id " << std::to_string(tid) << std::endl;
        return -1;
    }
    if (tid == MAIN_THREAD_ID) {
        free_uthreads();
        exit(0);
    } // otherwise, terminate thread:
	
	/* remove terminated thread from ready & blocked & used ids */
	ready.erase(std::remove(ready.begin(), ready.end(), tid), ready.end());
    blocked.erase(std::remove(blocked.begin(), blocked.end(), tid), blocked.end());

	/* add all the threads that finishied their sync dependency with this thread to the end of the ready threads list
	   and change their states */
	ready.insert(ready.cend(), uthreads[tid]->sync_dependencies.begin(), uthreads[tid]->sync_dependencies.end())
	auto is_blocked = [](int tid) { return std::find(blocked.begin(), blocked.end(), tid) != blocked.end(); }
	blocked.erase(std::remove_if(blocked.begin(), blocked.end(), is_blocked), blocked.end());

    if (current_thread == tid) {
        sigprocmask(SIG_UNBLOCK, &sa, NULL);
        contextSwitch(TERMINATE_SWITCH); // LO MEVIN
        return 0;
    }
	
    delete uthreads[tid];
    uthreads[tid] = nullptr;
	tids.remove_id(tid);
	sigprocmask(SIG_UNBLOCK, &sa, NULL);
}

/**
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered an error.
 * Return value: On success, return 0. On failure, return -1.
 * @param tid
 * @return On success, return 0. On failure, return -1.
 */
int uthread_block(int tid){
    if ((tid == MAIN_THREAD_ID) || (tid < 0) || (tid > MAX_THREAD_NUM) || (!unique_id.isIDUsed(tid))){
        return -1;
    } else if (current_thread == tid) {
        scheduling decision;
    } else {
        that_thread = uthreads[tid];
        that_thread.bloocked = true;
        blocked.pushback(tid);
        ready.erase(tid);
    }

}

/*
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
	sigemptyset(&sa);
    sigaddset(&sa, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &sa, NULL);

    if (!unique_id.is_id_valid(tid)) {
        std::cerr << "thread library error: resume called with invalid id " << std::to_string(tid) << std::endl;
        return -1;
    }
	if (tid == MAIN_THREAD_ID) { // main thread can't be blocked.
		return 0;
	}
	if (tid == current_thread) { // running thread isn't blocked
		return 0;
	}
	
	/* if thread is blocked, change it's state to ready */
	if (is_blocked(tid)) {
		blocked.erase( std::remove(blocked.begin(), blocked.end(), tid) , blocked.end());
		ready.push_back(tid);
	}
	
	sigprocmask(SIG_UNBLOCK, &sa, NULL);
}



/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid() {
	return current_thread;
}

/*
 * Description: This function returns the total number of quantums since
 * the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums() {
	return total_quantums;
}


/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid) {
	if (!unique_id.is_id_valid(tid)) {
        std::cerr << "thread library error: get_quantums called with invalid id " << std::to_string(tid) << std::endl;
        return -1;
    }
	return uthreads[tid]->quantums;
}