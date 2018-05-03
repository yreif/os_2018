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
    // https://codereview.stackexchange.com/questions/173618/generate-a-unique-numeric-identifier-for-each-instance-of-a-class
    // http://coliru.stacked-crooked.com/a/b531057532b3489d
public:

    static unsigned std::set<unsigned long> s_usedID;

    static unsigned long generateID() // Generate a valid ID
    {
        static unsigned long id = 0;

        while (Foo::isIDUsed(id)) // If all ID are taken, create an infinite loop!
            ++id;

        return id;
    }

    static void addID(unsigned long id) // Add a given ID to the set of used ID
    {
        s_usedID.insert(id);
    }

    static void removeID(unsigned long id) // Remove a given ID from the set of used ID
    {
        s_usedID.erase(id);
    }

    static bool isIDUsed(unsigned long id)
    {
        return s_usedID.count(id) == 1 ? true:false;
    }

    explicit Foo() : m_id(Foo::generateID())
    {
        Foo::addID(m_id); // ID is now taken
    }

    virtual ~Foo()
    {
        Foo::removeID(m_id); // Free the ID
    }

private:

    unsigned long m_id;
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