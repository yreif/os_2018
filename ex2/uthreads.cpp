

#include "uthreads.h"
#include <sys/time.h>
#include <csetjmp>
#include <cstdio>
#include <csignal>
#include <deque>
#include <set>
#include <iostream>
#include <algorithm>



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

typedef struct unique_id unique_id;
typedef class thread thread;

/** uthread class */
thread* uthreads[MAX_THREAD_NUM];
class thread {
public:
    bool blocked_directly; // blocked using uthreads_block on this thread
    bool sync_blocked; // blocked because it synced to another thread
    int quantums;
    std::deque<int> sync_dependencies; // all the threads that are synced with this one
    char *stack;
    thread() : blocked_directly(false), sync_blocked(false), quantums(0) {
        try{
            stack = new char[STACK_SIZE];
        }catch (const std::bad_alloc& e){
            throw(e);
        }
    }
    ~thread() {
        delete[] stack;
    }
};

/** unique_id struct - manages the id's */
struct unique_id {
private:
    int max_id = 0;
    std::set<int> unused_ids;
public:
    explicit unique_id(int _max_id) : max_id(_max_id) { // add all ids to set:
        auto iter = unused_ids.end();
        for (int i=1; i < max_id; ++i) {
            iter = unused_ids.insert(iter, i);
        }
    }

    /** returns the minimal id not in use if one exists, and -1 otherwise */
    int generate_id() {
        static int id;

        if (unused_ids.empty()) {
            return -1;
        }
        id = *unused_ids.begin();
        unused_ids.erase(unused_ids.begin());

        return id;
    }

    /** remove a certain id from the group of used ids */
    void remove_id(int id) {
        unused_ids.insert(id);
    }

    /** returns 0 if in use and 1 otherwise*/
    int is_used(int id) {
        return unused_ids.count(id) == 0 ? 1:0;
    }

    /** returns 0 if id is valid for library operation and 1 otherwise.
     * Main thread is considered valid. */
    int is_valid(int id) {
        return (id < max_id and id >= 0 and is_used(id) == 0) ? 0:1;
    }

    ~unique_id()
    {
        unused_ids.clear();
    }

};

/** global variables */
std::deque<int> ready;
int current_thread;
int total_quantums;
struct itimerval timer;
struct sigaction sa;
sigset_t set;
sigjmp_buf env[MAX_THREAD_NUM];
unique_id tids = unique_id(MAX_THREAD_NUM);


/* signal numbers in linux are numbers from 1 to 30, so we use negative numbers as
 * additional parameters to the function that handles switching threads */
#define TERMINATE_SWITCH (-2) // switch from terminated thread
#define BLOCK_SWITCH (-3) // switch from blocked thread
#define SAVEMASK (1) // for sigsetjmp, save sigmask



/**
 * Description: Releases the assigned library memory before exit.
 */
int uthreads_exit(int exit_code) {
    for (int tid=0; tid < MAX_THREAD_NUM; ++tid) {
        if (uthreads[tid]) {
            delete uthreads[tid];
            uthreads[tid] = nullptr;
        }
    }
    exit(exit_code);
}

/**
 * Description: Handles system errors.
 */
void uthreads_sys_error(const char * msg) {
    std::cerr << "system error: " << msg << std::endl;
    uthreads_exit(1);
}


/**
 * Description: Handles switching between threads in all cases.
 * */
void switch_threads(int signum) {
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
        uthreads_sys_error("sigaction error in function switch_threads");
    }
    std::cout << std::to_string(current_thread) << std::endl;
    int ret_val = sigsetjmp(env[current_thread], SAVEMASK);
    if (ret_val == 1) {
        std::cout << std::to_string(current_thread) << std::endl;
        std::cout << "heyy" << std::endl;

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
    if (signum == SIGVTALRM) {  // SIGVTALRM == 26
        ready.push_back(current_thread);
    }

    if (signum == TERMINATE_SWITCH) {
        delete uthreads[current_thread];
        uthreads[current_thread] = nullptr;
    }

    // update running state
    current_thread = ready.front();
    ready.pop_front();

    /** Each time switch is called, the newly running thread's quantum count increases. */
    uthreads[current_thread]->quantums += 1;
    total_quantums += 1;

    /** Setting the handler to switch_threads, so if the quantum for a thread ends we'll switch it */
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
        std::cerr << "thread library error: init called with non-positive quantum_usecs"
                  << std::endl;
        return -1;
    }
    int quantum_secs = 0;
    if (quantum_usecs / SECOND >= 1) {
        quantum_secs = (quantum_usecs / SECOND);
        quantum_usecs -= quantum_secs*SECOND;
    }
    // Configure the timer to expire after supplied time */
    timer.it_value.tv_sec = quantum_secs;		// first time interval, seconds part
    timer.it_value.tv_usec = quantum_usecs;		// first time interval, microseconds part
    // configure the timer to expire every quantum after that.
    timer.it_interval.tv_sec = quantum_secs;	// following time intervals, seconds part
    timer.it_interval.tv_usec = quantum_usecs;	// following time intervals, microseconds part

    sa.sa_handler = &switch_threads;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0) {
        uthreads_sys_error("sigaction error in function uthreads_init");
    }
    if (setitimer (ITIMER_VIRTUAL, &timer, nullptr)) {
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
    total_quantums = 1;
    main_thread->quantums = 1;
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
        std::cerr << "thread library error: request to spawn new thread failed, MAX_THREAD_NUM reached"
                  << std::endl;
        return -1;
    }
    try {
        new_thread = new thread();
    } catch (const std::bad_alloc& e) { // TO HAGAR: catch by reference
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        uthreads_sys_error("memory allocation error in uthread_spawn");
    }
    address_t sp, pc;
    sp = (address_t) new_thread->stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t)f;

    sigsetjmp(env[curr_id], SAVEMASK);

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
    sigprocmask(SIG_BLOCK, &set, nullptr);
    if (!tids.is_valid(tid)) {
        std::cerr << "thread library error: terminate called with invalid id " << std::to_string(tid)
                  << std::endl;
        return -1;
    }
    if (tid == MAIN_THREAD_ID) {
        uthreads_exit(0);
    } // otherwise, terminate thread:
    tids.remove_id(tid);
	/* remove terminated thread from ready */
	ready.erase(std::remove(ready.begin(), ready.end(), tid), ready.end());

	/* add all the threads that finished their sync dependency with this thread (and are not
	 * otherwise blocked) to the end of the ready threads list */
    for (int i : uthreads[tid]->sync_dependencies) {
        uthreads[i]->sync_blocked = false;
        if (!uthreads[i]->blocked_directly) { ready.push_back(i); }
    }

    if (current_thread == tid) {
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        switch_threads(TERMINATE_SWITCH);
        return 0;
    }
    delete uthreads[tid];
    uthreads[tid] = nullptr;
	sigprocmask(SIG_UNBLOCK, &set, nullptr);
    return 0;
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
        std::cerr << "thread library error: block called with invalid id " << std::to_string(tid)
                  << std::endl;
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        return -1;
    } else if (current_thread == tid) {
    /** If the running thread is blocking itself, a scheduling decision should be made */
        uthreads[tid]->blocked_directly = true;
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        switch_threads(BLOCK_SWITCH);
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
        std::cerr << "thread library error: resume called with invalid id " << std::to_string(tid)
                  << std::endl;
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
    if ( !tids.is_valid(tid) || current_thread == MAIN_THREAD_ID || tid == current_thread) {
        sigprocmask(SIG_UNBLOCK, &set, nullptr);
        std::cerr << "thread library error: sync called by thread " << std::to_string(current_thread) <<
                  " with invalid id " << std::to_string(tid) << std::endl;
        return -1;
    }
    uthreads[current_thread]->sync_blocked = true;
    uthreads[tid]->sync_dependencies.push_back(current_thread);
    sigprocmask(SIG_UNBLOCK, &set, nullptr);
    switch_threads(BLOCK_SWITCH);
    return 0;
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
	if (!tids.is_valid(tid)) {
        std::cerr << "thread library error: get_quantums called with invalid id " << std::to_string(tid)
                  << std::endl;
        return -1;
    }
	return uthreads[tid]->quantums;
}