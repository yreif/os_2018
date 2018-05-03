#include "uthreads.h"
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>


#define SECOND 1000000
#define MAIN_THREAD_ID 0

enum state {
    READY, RUNNING, BLOCKED
};

typedef struct unique_id {

public:

    static std::set<int> s_unUsedID;

    static int generateID() // Generate a valid ID
    {
        static int id = 0;

        if (s_unUsedID.begin() == s_unUsedID.end()) {
            return error.
        }
        id = *s_unUsedID.begin();
        s_unUsedID.erase(id);

        return id;
    }

    static void start_me(int id) // Add a given ID to the set of used ID
    {
        for (int i = 1; i < MAIN_THREAD_ID + 1; ++i) {
            s_unUsedID.insert(i);
        }
    }

    static void removeID(int id) // Remove a given ID from the set of used ID
    {
        s_unUsedID.insert(id);
    }

    static bool isIDUsed(int id)
    {
        if (id > MAX_THREAD_NUM)
        {
            return error.
        }
        return s_unUsedID.count(id) == 0;
    }

} unique_id;



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
    if (tid == MAIN_THREAD_ID) {
        exit(0);
    }


}