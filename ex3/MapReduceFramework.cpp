#include <pthread.h>
#include <cstdio>
#include <atomic>
#include "MapReduceFramework.h"
#include "Barrier.h"
#include <algorithm> // for std::sort, std::max_element
#include <iostream>
#include <semaphore.h>


typedef std::vector<IntermediateVec> IntermediateVectors;
typedef std::vector<IntermediateVec*> Queue;

struct ThreadContext {
    const MapReduceClient* client;
    const InputVec* inputVector;
    OutputVec* outputVec;
    IntermediateVectors* intermediateVectors;
    Queue* shuffleVectors;
    int threadID;
    Barrier* barrier;
    std::atomic<unsigned long>* inputVectorIndex;
    sem_t* semaphore;
    pthread_mutex_t* shuffle_mutex;
    pthread_mutex_t* reduce_mutex;
    std::atomic<bool>* isShuffleDone;
};

/** Helper functions */

/**
 * Compares two IntermediatePairs by comparing their keys
 */
bool intermediatePairComp(IntermediatePair& lhs, IntermediatePair& rhs) {
    return *lhs.first < *rhs.first;
}

/**
 * Checks if two K2's are equal.
 */
bool keyEqual2(K2* a, K2* b){
    return !(*a < *b) && !(*b < *a);
}

/**
 * Returns the largest key found in the given IntermediateVectors
 */
K2 *getMaxKey(IntermediateVectors *vectors) {
    auto first = vectors->begin();
    auto last = vectors->end();
    if (first == last) {
        return first->back().first;
    }
    auto largest = first;
    ++first;
    for (; first != last; ++first) {
        if (*largest->back().first < *first->back().first) {
            largest = first;
        }
    }
    return largest->back().first;

}

/**
 * Returns a new IntermediateVec containing all of the pairs with the largest key found in
 * the given vector of IntermediateVec's, while removing them from their original IntermediateVec's.
 */
IntermediateVec *createShuffleVec(IntermediateVectors * vectors) {
    K2 *currKey = getMaxKey(vectors);
    auto shuffleVec = new IntermediateVec();
    for (auto vecIter = vectors->begin(); vecIter != vectors->end(); ) {
        if (*(*vecIter).back().first < *currKey) {
            ++vecIter;
            continue;
        }
        bool visitedKey = false;
        for (auto keyIter = vecIter->rbegin(); keyIter != vecIter->rend(); ++keyIter ) {
            if (keyEqual2(keyIter->first, currKey)) {
                visitedKey = true;
                shuffleVec->push_back(*keyIter);
                vecIter->pop_back();
            } else if (visitedKey) {
                break;
            }
        }
        if (vecIter->empty()) {
            vecIter = vectors->erase(vecIter);
        } else {
            ++vecIter;
        }
    }
    return shuffleVec;
}

/**
 * handles errors from threads.
 * @param ret return value of system call
 * @param sysCall which system call was used
 * @param f in which function/phase was this system call called
 * @param tc thread context of the thread that called the system call
 */
void err(int ret, const char *sysCall, const char *f, ThreadContext *tc) {
    if (ret == 0) {
        return;
    }
    std::cerr << "Thread " << tc->threadID << ": error in " << sysCall << " during " << f << std::endl;
    exit(1);
}

/**
 * handles general errors.
 * @param ret return value of system call
 * @param sysCall which system call was used
 */
void err(int ret, const char *sysCall) {
    if (ret == 0) {
        return;
    }
    std::cerr << "Error in " << sysCall << " during runMapReduceFramework" << std::endl;
    exit(1);
}

/** MapReduceFramework phases */

void mapPhase(ThreadContext *tc) {
    unsigned long inputSize = tc->inputVector->size();
    unsigned long previousIndex = 0;
    while (true) {
        previousIndex = (*tc->inputVectorIndex)++;
        if (previousIndex < inputSize) {
            const InputPair& pair = (*tc->inputVector)[previousIndex];
            // pair.first = K1, pair.second = V1
            tc->client->map(pair.first, pair.second, tc);
        } else {
            break;
        }
    }
}

void sortPhase(ThreadContext *tc) {
    IntermediateVec &intermediateVec = (*tc->intermediateVectors)[tc->threadID];
    sort(intermediateVec.begin(), intermediateVec.end(), intermediatePairComp);
    tc->barrier->barrier();
}

void shufflePhase(ThreadContext *tc) {
    IntermediateVectors * vectors = tc->intermediateVectors;
    unsigned long threadNum = tc->intermediateVectors->size();
    IntermediateVectors::iterator maxVec;

    // erase empty intermediate vectors:
    vectors->erase(std::remove_if(vectors->begin(), vectors->end(),
                                  [](const IntermediateVec& v) { return v.empty(); }), vectors->end());

    while (!vectors->empty()) {
        IntermediateVec *shuffleVec = createShuffleVec(vectors);
        // lock shuffle_mutex to add a new vector to queue and notify waiting threads
        err(pthread_mutex_lock(tc->shuffle_mutex), "pthread_mutex_lock", "shuffle phase", tc);
        (*tc->shuffleVectors).push_back(shuffleVec);
        err(pthread_mutex_unlock(tc->shuffle_mutex), "pthread_mutex_unlock", "shuffle phase", tc);
        err(sem_post(tc->semaphore), "sem_post", "shuffle phase", tc);
    }

    *tc->isShuffleDone = true;
    for (unsigned long i = 0; i < threadNum; ++i) { // wake up any remaining waiting threads
        err(sem_post(tc->semaphore), "sem_post", "shuffle phase", tc);
    }
}

void reducePhase(ThreadContext *tc) {
    while (true) {
        bool queueIsEmpty;
        IntermediateVec* reduceVec = nullptr;

        err(sem_wait(tc->semaphore), "sem_wait", "reduce phase", tc); // sem_wait
        // lock shuffle_mutex - check if the queue is empty. if not, take a vector to reduce.
        err(pthread_mutex_lock(tc->shuffle_mutex), "pthread_mutex_lock", "reduce phase", tc);
        queueIsEmpty = tc->shuffleVectors->empty();
        if (!queueIsEmpty) {
            reduceVec = (*tc->shuffleVectors).back();
            (*tc->shuffleVectors).pop_back();
        }
        err(pthread_mutex_unlock(tc->shuffle_mutex), "pthread_mutex_unlock", "reduce phase", tc);
        if (queueIsEmpty && *tc->isShuffleDone) {
            break;
        }
        else {
            tc->client->reduce(reduceVec, tc);
            delete(reduceVec);
        }
    }
}

/**
 * The main function run by each thread.
 */
void *singleThread(void *arg) {
    auto* tc = (ThreadContext*) arg;
    mapPhase(tc);
    sortPhase(tc);
    if (tc->threadID == 0) {
        shufflePhase(tc);
    }
    reducePhase(tc);

    return nullptr;
}
/** Library Fucntions */

/**
 * This function produces a (K2*,V2*) pair.
 */
void emit2 (K2* key, V2* value, void* context) {
    auto* tc = (ThreadContext*) context;
    IntermediatePair pair = IntermediatePair(key, value);
    (*tc->intermediateVectors)[tc->threadID].push_back(pair);
}

/**
 * This function produces a (K3*,V3*) pair.
 */
void emit3 (K3* key, V3* value, void* context) {
    auto* tc = (ThreadContext*) context;
    OutputPair pair = OutputPair(key, value);

    err(pthread_mutex_lock(tc->reduce_mutex), "pthread_mutex_lock", "emit3", tc);
    (*tc->outputVec).push_back(pair);
    err(pthread_mutex_unlock(tc->reduce_mutex), "pthread_mutex_unlock", "emit3", tc);
}

/**
 * This function starts runs the entire MapReduce algorithm.
 * @param client The implementation of MapReduceClient or in other words the task that the
          framework should run.
 * @param inputVec the input elements
 * @param outputVec to which the output
          elements will be added before returning
 * @param multiThreadLevel the number of threads that should be created.
 */
void runMapReduceFramework(const MapReduceClient& client,
                           const InputVec& inputVec, OutputVec& outputVec,
                           int multiThreadLevel) {
    pthread_t threads[multiThreadLevel-1];
    ThreadContext contexts[multiThreadLevel];
    Barrier barrier(multiThreadLevel);
    IntermediateVectors intermediateVectors(multiThreadLevel);
    Queue shuffleVectors(0);
    std::atomic<unsigned long> inputIndex(0);
    sem_t sem;
    std::atomic<bool> isShuffleDone(false);
    err(sem_init(&sem, 0, 0), "sem_init");

    pthread_mutex_t shuffle_mutex, reduce_mutex;

    err(pthread_mutex_init(&shuffle_mutex, nullptr), "pthread_mutex_init");
    err(pthread_mutex_init(&reduce_mutex, nullptr), "pthread_mutex_init");

    for (int id = 0; id < multiThreadLevel; ++id) {
        contexts[id] = {&client, &inputVec, &outputVec, &intermediateVectors, &shuffleVectors,
                       id, &barrier, &inputIndex, &sem, &shuffle_mutex, &reduce_mutex, &isShuffleDone};
    }

    for (int i = 0; i < multiThreadLevel-1; ++i) {
        err(pthread_create(threads + i, nullptr, singleThread, contexts + i), "pthread_create");
    }

    singleThread(&contexts[multiThreadLevel - 1]);

    for (int i = 0; i < multiThreadLevel-1; ++i) {
        err(pthread_join(threads[i], nullptr), "pthread_join");
    }

    err(pthread_mutex_destroy(&shuffle_mutex), "pthread_mutex_destroy");
    err(pthread_mutex_destroy(&reduce_mutex), "pthread_mutex_destroy");
    err(sem_destroy(&sem), "sem_destroy");
}


