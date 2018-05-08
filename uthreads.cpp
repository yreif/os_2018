
// TO HAGAR: goodtoknow - in C++ use <cstdlib> and not <stdlib.h>
#include "uthreads.h"
#include <sys/time.h>
#include <csetjmp>
#include <cstdio>
#include <csignal>
#include <deque>
#include <set>
#include <iostream>
#include <algorithm>
#include <cstdlib>

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
std::deque<int> ready;
//std::deque<int> blocked;  // TO HAGAR: I check all uses of this and it's better to just use another field in thread
int current_thread;
int total_quantums;

struct itimerval timer;
struct sigaction sa;
sigset_t set; // TODO: is it okay to make this global? TO HAGAR: seems okay 'cus sigprocmask requires it to be const
sigjmp_buf env[MAX_THREAD_NUM];

// TO HAGAR: when functions were static they couldn't do stuff to the set. also change set's name to something with _
typedef struct unique_id {
private:
    int max_id = 0;
    std::set<int> unused_ids;
public:
    explicit unique_id(int _max_id) : max_id(_max_id) {
        auto iter = unused_ids.end();
        for (int i=1; i < max_id; ++i) {
            iter = unused_ids.insert(iter, i);
        }
    }

    int generate_id()
    {
        static int id = 0;

        if (unused_ids.empty()) {
            return -1;
        }
        id = *unused_ids.begin();
        unused_ids.erase(unused_ids.begin()); // TO HAGAR: erase by position is O(1), by value is O(log n)

        return id;
    }

    void remove_id(int id) // Remove a given ID from the set of used ID
    {
        unused_ids.insert(id);
    }

    /** returns 0 if in use and 1 otherwise*/
    int is_used(int id)
    {
        return unused_ids.count(id) == 0 ? 1:0;
    }

    /** returns 0 if id is valid for library operation and 1 otherwise Doesn't check if it is the Main thread,*/
    int is_valid(int id)
    {
        return (id < max_id and id >= 0 and is_used(id) == 0) ? 0:1;
    }

} unique_id; // TODO: check for every function that id checking fits it

unique_id tids = unique_id(MAX_THREAD_NUM);
#define SELF_SWITCH -30
#define TERMINATE_SWITCH -40
#define RUNNING_BLOCKED_SWITCH -50
#define SYNCED_BLOCKED_SWITCH -60
#define NO_THREAD -10

/** uthread class */
typedef class thread thread;
thread* uthreads[MAX_THREAD_NUM];

/** This c */
class thread {
public:
    int quantums;
    std::deque<int> sync_dependencies;     // thread ids waiting for this thread to run // UPDATE THIS
    int waiting_for;                   // current thread is waiting for synced_t to run // UPDATE THIS
    bool blocked_directly;
    bool sync_blocked;
    char *stack;
    thread() : quantums(0), blocked_directly(false), sync_blocked(false), waiting_for(NO_THREAD), stack(nullptr) {
    }
    ~thread() {
        delete[] stack;
    }
};


/*
 * Description: Releases the assigned library memory before exit
 */
int uthreads_exit(int exit_code) {
    for (int tid=1; tid < MAX_THREAD_NUM; ++tid) {
        if (uthreads[tid]) {
            delete uthreads[tid];
            uthreads[tid] = nullptr;
        }
    }
    exit(exit_code);
    // TODO: handle deletion of main thread if necessary
}

/*
 * Description: Handles system errors
 */
void uthreads_sys_error(const char * msg) {
    std::cerr << "system error: " << msg << std::endl;
    uthreads_exit(1);
}


/** Setting the handler to ignore the default alarm signal */
void switch_threads(int status) {
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
        uthreads_sys_error("sigaction error in function switch_threads");
    }

    int ret_val = sigsetjmp(env[current_thread],1);
    if (ret_val == 1) {
        sa.sa_handler = &switch_threads;
        if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
            uthreads_sys_error("sigaction error in function switch_threads");
        }
        /** initialize timer */
        if (setitimer (ITIMER_VIRTUAL, &timer, nullptr)) {
            uthreads_sys_error("setitimer error in function switch_threads");
        }
        return;
    } /** otherwise, thread not preempted: insert back into ready queue */
    if (status == SIGVTALRM) {
        ready.push_back(current_thread);
    }

    if (status == TERMINATE_SWITCH){
        delete uthreads[current_thread];
        uthreads[current_thread] = nullptr;
    }

    // update running state
    current_thread = ready.front();
    ready.pop_front();

    /**Each time switch is called the quantums increase. */
    uthreads[current_thread]->quantums += 1;
    total_quantums += 1;

    /**Setting the current handler to be switchThread, such that if the timer for a thread ends, we'll switch it */
    sa.sa_handler = &switch_threads;
    sigaction(SIGVTALRM, &sa, nullptr);
    if (setitimer (ITIMER_VIRTUAL, &timer, nullptr)) {
        uthreads_sys_error("setitimer error in function switch_threads");
    }

    siglongjmp(env[current_thread],1);
}

/**
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an error to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);

    if (quantum_usecs <= 0){
        std::cerr << "thread library error: init called with non-positive quantum_usecs" << std::endl;
        return -1;
    }
    int quantum_secs = 0;
    if (quantum_usecs / SECOND >= 1) { // TODO: check if this works well or matters at all
        quantum_secs = (int) (quantum_usecs / SECOND);
        quantum_usecs -= quantum_secs*SECOND;
    }
    // Configure the timer to expire after supplied time */
    timer.it_value.tv_sec = quantum_secs;		// first time interval, seconds part
    timer.it_value.tv_usec = quantum_usecs;		// first time interval, microseconds part
    // configure the timer to expire every quantum after that.
    timer.it_interval.tv_sec = quantum_secs;	// following time intervals, seconds part
    timer.it_interval.tv_usec = quantum_usecs;	// following time intervals, microseconds part

    sa.sa_handler = &switch_threads;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
        uthreads_sys_error("sigaction error in function uthreads_init");
    }
    if (setitimer (ITIMER_VIRTUAL, &timer, NULL)) {
        uthreads_sys_error("setitimer error in function uthreads_init");
    }
    current_thread = 0;
    thread* main_thread = nullptr;
    try
    {
        main_thread = new thread();
    }
    catch (const std::bad_alloc& e)
    {
        std::cerr << "system error: memory allocation failed.\n" << std::endl;
        exit(1);
    }
    uthreads[0] = main_thread;
    return 0;
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

    sigprocmask(SIG_BLOCK, &set, nullptr);

    thread* new_thread = nullptr;
    int curr_id = tids.generate_id();
    if (curr_id == -1) {
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        std::cerr << "thread library error: request to spawn new thread failed, MAX_THREAD_NUM reached" << std::endl;
        return -1;
    }
    try {
        new_thread = new thread();
        new_thread->stack = new char[STACK_SIZE];

    } catch (const std::bad_alloc& e) { // TO HAGAR: catch by reference
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        uthreads_sys_error("memory allocation error in uthread_spawn");
    }
    address_t sp, pc;
    sp = (address_t) new_thread->stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t)f;

    // store thread's context and return for first context switch
//    int ret_val =   // they didn't do this in the demo
    sigsetjmp(env[curr_id], 1);
//    if (ret_val != 0){  // they didn't do this in the demo. what is this?
//        sa.sa_handler = &contextSwitch;
//        sigaction(SIGVTALRM, &sa, nullptr);
//        if (setitimer (ITIMER_VIRTUAL, &timer, nullptr)) {
//            std::cerr << "system error: setitimer error in function uthread_spawn" << std::endl;
//        }
//    }
    (env[curr_id]->__jmpbuf)[JB_SP] = translate_address(sp);
    (env[curr_id]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env[curr_id]->__saved_mask);

    uthreads[curr_id] = new_thread;
    ready.push_back(curr_id);

    sigprocmask(SIG_UNBLOCK, &set, nullptr);
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
    // TODO: make sure it is okay to not block signals in the error-checking section!
    if (!tids.is_valid(tid)) {
        std::cerr << "thread library error: terminate called with invalid id " << std::to_string(tid) << std::endl;
        return -1;
    }
    if (tid == MAIN_THREAD_ID) {
        uthreads_exit(0);
    } // otherwise, terminate thread:
    sigprocmask(SIG_BLOCK, &set, nullptr);
    tids.remove_id(tid);
	/* remove terminated thread from ready */
	ready.erase(std::remove(ready.begin(), ready.end(), tid), ready.end());
//    blocked.erase(remove(blocked.begin(), blocked.end(), tid), blocked.end());

	/* add all the threads that finished their sync dependency with this thread (and are not otherwise blocked)
	 * to the end of the ready threads list */
    for (int i : uthreads[tid]->sync_dependencies) {
        uthreads[i]->sync_blocked = false;
        if (!uthreads[i]->blocked_directly) { ready.push_back(i); }
    }
//    auto change_states = [](int i) {
//        uthreads[i]->sync_blocked = false;
//        if (!uthreads[i]->blocked_directly) { ready.push_back(i); }
//    };
//	ready.insert(ready.cend(), uthreads[tid]->sync_dependencies.begin(), uthreads[tid]->sync_dependencies.end());
//	blocked.erase(std::remove_if(blocked.begin(), blocked.end(), is_sync_blocked), blocked.end());

    if (current_thread == tid) {
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        switch_threads(TERMINATE_SWITCH); // LO MEVIN, maybe better to do this here
        return 0;
    }

    delete uthreads[tid];
    uthreads[tid] = nullptr;
	sigprocmask(SIG_UNBLOCK, &set, nullptr);
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
int uthread_block(int tid) {
    sigprocmask(SIG_BLOCK, &set, nullptr);
    /** If the ID is invalid */
    if (!tids.is_valid(tid) || tid == MAIN_THREAD_ID) {
        std::cerr << "thread library error: block called with invalid id " << std::to_string(tid) << std::endl;
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        return -1;
    } else if (current_thread == tid) {
    /** If the running thread is blocking itself, a scheduling decision should be made */
        uthreads[tid]->blocked_directly = true;
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        switch_threads(RUNNING_BLOCKED_SWITCH); // TODO: do we still use this state?
    } else if (!uthreads[tid]->blocked_directly) { /** thread is not already blocked: */
        uthreads[tid]->blocked_directly = true;
        ready.erase(std::remove(ready.begin(), ready.end(), tid), ready.end());
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
    }
    return 0;

}

/**
 * Description: This function resumes a blocked thread with ID tid and moves
 * it to the READY state if it's not synced. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
    sigprocmask(SIG_BLOCK, &set, nullptr);

    if (!tids.is_valid(tid)) {
        std::cerr << "thread library error: resume called with invalid id " << std::to_string(tid) << std::endl;
        return -1;
    }
    // main thread can't be blocked, running thread surely isn't blocked:
    if (tid == MAIN_THREAD_ID or tid == current_thread) {
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        return 0;
    }

    /* if thread is blocked, change it's state to ready */
    if (uthreads[tid]->blocked_directly) {
        uthreads[tid]->blocked_directly = false;
        if (!uthreads[tid]->sync_blocked) {
            ready.push_back(tid);
        }
    }

    sigprocmask(SIG_UNBLOCK, &set, nullptr);
    return 0;
}

/**
 * Description: This function blocks the RUNNING thread until thread with
 * ID tid will terminate. It is considered an error if no thread with ID tid
 * exists, if thread tid calls this function or if the main thread (tid==0) calls this function.
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision
 * should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sync(int tid) {
    sigprocmask(SIG_BLOCK, &set, nullptr);
    if (tids.is_valid(tid) || current_thread == MAIN_THREAD_ID || tid == current_thread) {
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        std::cerr << "thread library error: sync called by thread " << std::to_string(current_thread) <<
                  " with invalid id " << std::to_string(tid) << std::endl;
        return -1;
    }
    uthreads[current_thread]->sync_blocked = true;
    uthreads[current_thread]->waiting_for = tid;
    uthreads[tid]->sync_dependencies.push_back(current_thread);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
    switch_threads(SYNCED_BLOCKED_SWITCH);
    return 0; // TODO: when does this function return? is it okay to free block b4 switching?
}

/**
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid() {
	return current_thread;
}

/**
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

/**
 * Description: This fundction returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered an error.
 * Return value: On success, return the number of quantums of the thread with ID tid.
 * 			     On failure, return -1.
*/
int uthread_get_quantums(int tid) { // TODO: make sure this works okay on main thread
	if (!tids.is_valid(tid)) {
        std::cerr << "thread library error: get_quantums called with invalid id " << std::to_string(tid) << std::endl;
        return -1;
    }
	return uthreads[tid]->quantums;
}