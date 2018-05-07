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

typedef unsigned int address_t;  // To UPDATE NAME??
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
std::deque<int> blocked_by_sync;
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
#define SYNCED_BLOCKED_SWITCH -60
#define NO_THREAD -10

/** uthread class */
typedef class thread thread;
thread* uthreads[MAX_THREAD_NUM];
struct sigaction sa;

/** */
class thread {
public:
    int quantums;
    std::deque<int> sync_dependencies;     // thread ids waiting for this thread to run // UPDATE THIS
    int waiting_for;                   // current thread is waiting for synced_t to run // UPDATE THIS
    bool blocked_directly;
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

/** */
void switchThreads(int status) {
    /**Setting the handler to ignore the default alarm signal */
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
        printf("sigaction error.");
    }

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

    if (status == TERMINATE_SWITCH){
        delete uthreads[runningIndex];
        uthreads[runningIndex] = nullptr;
    }

    // update running state
    current_thread = ready.front();
    ready.pop_front();

    /**Each time switch is called the quantums increase. */
    uthreads[current_thread]->quantums += 1;
    total_quantums += 1;

    /**Setting the current handler to be switchThread, such that if the timer for a thread ends, we'll switch it */
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

    sigemptyset(&sa);
    sigaddset(&sa, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &sa, NULL);

    int curr_id = unique_id.generateID();
    if(curr_id == -1){
        sigprocmask(SIG_UNBLOCK, &sa, NULL);
        std::cerr << "Error\n" << std::endl;
        return curr_id;
    }

    try {
        uthread* new_thread = new thread();
        new_thread->stack = new char[STACK_SIZE];

    } catch (std::bad_alloc){
        sigprocmask(SIG_UNBLOCK, &sa, NULL);
        std::cerr << "Error\n" << std::endl;
        ~new_thread();
        return -1;
    }

    ////////////////// FROM HERE SAME AS KERENS

    address_t sp, pc;
    sp = (address_t)new_thread->stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t)f;

    // store thread's context and return for first context switch
    int ret_val = sigsetjmp(env[tid], 1);
    if (ret_val != 0){
        sa.sa_handler = &contextSwitch;
        sigaction(SIGVTALRM, &sa, NULL);
        if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
            std::cerr << "Error\n" << std::endl;
            // no error thrown, program will continue running with timing error
            // in order to not disrupt flow
        }
        return;
    }
    (env[tid]->__jmpbuf)[JB_SP] = translate_address(sp);
    (env[tid]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env[tid]->__saved_mask);

    uthreads[curr_id] = curr_thread;
    ready.pushback(curr_id);
    sigprocmask(SIG_UNBLOCK, &sa, NULL);


    return curr_id;
}

/** */
bool is_blocked(int tid) {
	d
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
	tids.remove_id(tid);

	/* add all the threads that finishied their sync dependency with this thread to the end of the ready threads list
	   and change their states */
	ready.insert(ready.cend(), uthreads[tid]->sync_dependencies.begin(), uthreads[tid]->sync_dependencies.end());
	blocked.erase(std::remove_if(blocked.begin(), blocked.end(), [](decltype(blocked)::value_type const& elem) {
                                          return elem.z < 0;
                                      }), blocked.end());
	
    // clear all syncs
    for (int syncedThread : threadPtrs[tid]->syncedWith)
    {
        threadPtrs[syncedThread]->synced_t = NO_THREAD;
        if (!threadPtrs[syncedThread]->blocked){
            for (int i = (int)blocked.size(); i >= 0; --i)
            {
                if (blocked[i] == syncedThread)
                {
                    blocked.erase(blocked.begin() + i);
                    break;
                }
            }
            ready.push_back(syncedThread);
        }
    }

    if (runningIndex == tid){
        sigprocmask(SIG_UNBLOCK, &set, NULL);
        contextSwitch(TERMINATE_SWITCH);
        return 0;
    } else{
        delete threadPtrs[tid];
        threadPtrs[tid] = nullptr;
    }
	
	
	
	
	
	
    if (get_thread(tid, to_terminate) < 0) { // MISSING IMPLEMENTATION
        uthreads_error("no thread with ID tid exists");
        return -1;
    }
    // how to know if a thread terminates itself???
    to_terminate.terminate();
	
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

    sigemptyset(&sa);
    sigaddset(&sa, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &sa, NULL);

    /**If the ID is invalid */
    if (unique_id.is_id_valid(tid) || tid == MAIN_THREAD_ID){
        sigprocmask(SIG_UNBLOCK, &sa, NULL);
        std::cerr << "Error\n" << std::endl;
        return -1;
    } else if (current_thread == tid){
    /**If the current thread - the running one is blocking itself, a schedualing decision should be made */

        current_thread->blocked_directly = true;
        sigprocmask(SIG_UNBLOCK, &sa, NULL);
        switchThreads(RUNNING_BLOCKED_SWITCH);

    } else if (!uthreads[tid]->blocked_directly){
        that_thread = uthreads[tid];
        that_thread->blocked_directly = true;
        ready.erase(std::remove(ready.begin(), ready.end(), tid), ready.end());
        sigprocmask(SIG_UNBLOCK, &sa, NULL);

    }
    return 0;

}

/**
 * Description: This function blocks the RUNNING thread until thread with
 * ID tid will terminate. It is considered an error if no thread with ID tid
 * exists or if the main thread (tid==0) calls this function. Immediately after the
 * RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sync(int tid) {

    sigemptyset(&sa);
    sigaddset(&sa, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &sa, NULL);

    /**If the ID is invalid , Or if it belonges to the running thread - then not logical*/
    if (unique_id.is_id_valid(tid) || current_thread == MAIN_THREAD_ID || tid == current_thread){
        sigprocmask(SIG_UNBLOCK, &sa, NULL);
        std::cerr << "Error\n" << std::endl;
        return -1;
    }

    uthread* t_sync = uthreads[tid];
    t_sync.sync_dependencies.pushback(tid);
    uthreads[current_thread]->waiting_for = tid;

    blocked_by_sync.pushback(tid);
    switchThreads(SYNCED_BLOCKED_SWITCH);
    sigprocmask(SIG_UNBLOCK, &sa, NULL);

    return 0;
}


/*
 * Description: Releases the assigned library memory.
*/
int free_uthreads() {

}