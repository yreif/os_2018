#include <pthread.h>
#include <cstdio>
#include <atomic>
#include "MapReduceFramework.h"
#include "Barrier.h" // Yuval: TODO: Barrier class needs to be updated to have return value checks for shuffle_mutex (in case of error)
#include <algorithm> // for std::sort
#include <iostream> // Yuval: TODO: remove after testing
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

bool vectorComp(IntermediateVec& lhs, IntermediateVec& rhs) {
    return lhs[0] < rhs[0];
}

bool intermediatePairComp(IntermediatePair& lhs, IntermediatePair& rhs) {
    return *lhs.first < *rhs.first;
}

bool keyEqual2(K2* a, K2* b){
    return !(*a < *b) && !(*b < *a);
}

void errorHandler(int ret, const char* sysCall, const char* f, ThreadContext* tc) {
    if (ret == 0) {
        return;
    }
    std::cerr << "Thread " << tc->threadID << ": error in " << sysCall << " during " << f << std::endl;
    exit(1);
}

void emit2 (K2* key, V2* value, void* context) {
    auto* tc = (ThreadContext*) context;
    IntermediatePair pair = IntermediatePair(key, value);
    (*tc->intermediateVectors)[tc->threadID].push_back(pair);
}

void emit3 (K3* key, V3* value, void* context) {
    auto* tc = (ThreadContext*) context;
    OutputPair pair = OutputPair(key, value);

    errorHandler(pthread_mutex_lock(tc->reduce_mutex), "pthread_mutex_lock", "emit3", tc);
    (*tc->outputVec).push_back(pair);
    errorHandler(pthread_mutex_unlock(tc->reduce_mutex), "pthread_mutex_unlock", "emit3", tc);
}


//void properExit(pthread_t* threads, ThreadContext contexts[]){
//    for (int i = 0; i < ; ++i) {
//
//    }
//
//}

void mapPhase(ThreadContext *tc) {
    /** Map phase: Take input from inputVector */
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
    K2* currKey = nullptr;

    // erase empty intermediate vectors:
    vectors->erase(std::remove_if(vectors->begin(), vectors->end(),
                                  [](const IntermediateVec& v) { return v.empty(); }), vectors->end());

    while (!vectors->empty()) {
        /**
        for (auto sortedVecIter = vectors->begin(); sortedVecIter != vectors->end(); ) {
            if (sortedVecIter->empty()) {
                sortedVecIter = vectors->erase(sortedVecIter);
            } else {
                sortedVecIter++;
            }
        }
        if (vectors->empty()) {
            break;
        } */ // TODO: delete
        sort(vectors->begin(), vectors->end(), vectorComp);
        currKey = ((*vectors)[0])[0].first;
        auto shuffleVec = new IntermediateVec();
        for (auto vecIter = vectors->begin(); vecIter != vectors->end(); ) {
            if (*currKey < *(*vecIter)[0].first) {
                break;
            }
            bool visitedKey = false;
            for (auto keyIter = vecIter->begin(); keyIter != vecIter->end(); ) {
                if (keyEqual2(keyIter->first, currKey)) {
                    visitedKey = true;
                    shuffleVec->push_back(*keyIter);
                    keyIter = vecIter->erase(keyIter);
                } else if (visitedKey) {
                    break;
                } else {
                    ++keyIter;
                }
            }
            if (vecIter->empty()) {
                vecIter = vectors->erase(vecIter);
            } else {
                vecIter++;
            }
        }
        errorHandler(pthread_mutex_lock(tc->shuffle_mutex), "pthread_mutex_lock", "shuffle phase", tc);
        (*tc->shuffleVectors).push_back(shuffleVec);
        errorHandler(pthread_mutex_unlock(tc->shuffle_mutex), "pthread_mutex_unlock", "shuffle phase", tc);
        errorHandler(sem_post(tc->semaphore), "sem_post", "shuffle phase", tc);
    }
    // TODO: to Hagar - I moved the "Shuffle done" updated to here, before the "trick" and it seems like it fixed it
    *tc->isShuffleDone = true;
    for (unsigned long i = 0; i < threadNum; ++i) {
        errorHandler(sem_post(tc->semaphore), "sem_post", "shuffle phase", tc);
    }
}

void reducePhase(ThreadContext *tc) {
    while (true) {
        bool queueIsEmpty;
        IntermediateVec* reduceVec = nullptr;

        errorHandler(sem_wait(tc->semaphore), "sem_wait", "reduce phase", tc);
        errorHandler(pthread_mutex_lock(tc->shuffle_mutex), "pthread_mutex_lock", "reduce phase", tc);
        queueIsEmpty = tc->shuffleVectors->empty();
        if (!queueIsEmpty) {
            reduceVec = (*tc->shuffleVectors).back();
            (*tc->shuffleVectors).pop_back();
        }
        errorHandler(pthread_mutex_unlock(tc->shuffle_mutex), "pthread_mutex_unlock", "reduce phase", tc);
        if (queueIsEmpty && *tc->isShuffleDone) {
            break;
        }
        else {
            tc->client->reduce(reduceVec, tc);
            delete(reduceVec);
        }
    }
}

//    while (true) {
//        int sem_val;
//        bool queueIsEmpty;
//        IntermediateVec* reduceVec = nullptr;
//
//        sem_getvalue(tc->semaphore, &sem_val);
//        if (sem_val > 0) {
//            sem_wait(tc->semaphore);
//
//            pthread_mutex_lock(tc->shuffle_mutex);
//            queueIsEmpty = tc->shuffleVectors->empty();
//            if (!queueIsEmpty) {
//                reduceVec = (*tc->shuffleVectors).back();
//                (*tc->shuffleVectors).pop_back();
//            }
//            pthread_mutex_unlock(tc->shuffle_mutex);
//            if (!queueIsEmpty) {
//                tc->client->reduce(reduceVec, tc);
//                delete(reduceVec);
//            }
//
//
//        } else {
//            break;
//        }
//
//    }


void *singleThread(void *arg) {

    /** testing Map and Sort phases:
    char *c;
    int *i;

    for (int id = 0; id < tc->intermediateVectors->size(); ++id) {
        std::cout << "thread " << std::to_string(id) << "'s intermediateVec:" << std::endl;
        for (const auto pair : (*tc->intermediateVectors)[id]) {
            std::cout << "(" << pair.first->as_string() << ", " << pair.second->as_string() << ")" << std::endl;
        }
        std::cout << std::endl;
    }
     end of testing section */

    auto* tc = (ThreadContext*) arg;
    mapPhase(tc);
    sortPhase(tc);
    if (tc->threadID == 0) {
        shufflePhase(tc);
    }
    reducePhase(tc);

    return nullptr;

}


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

    if (sem_init(&sem, 0, 0) != 0) {
        std::cerr << "Error on sem_init" << std::endl;
        exit(1);
    }

    pthread_mutex_t shuffle_mutex, reduce_mutex;
    if (pthread_mutex_init(&shuffle_mutex, nullptr) != 0 or pthread_mutex_init(&reduce_mutex, nullptr) != 0) {
        std::cerr << "Error on pthread_mutex_init" << std::endl;
        exit(1);
    }


    for (int id = 0; id < multiThreadLevel; ++id) {
        contexts[id] = {&client, &inputVec, &outputVec, &intermediateVectors, &shuffleVectors,
                       id, &barrier, &inputIndex, &sem, &shuffle_mutex, &reduce_mutex, &isShuffleDone};
    }

    for (int i = 0; i < multiThreadLevel-1; ++i) {
        if (pthread_create(threads + i, nullptr, singleThread, contexts + i) != 0) {
            std::cerr << "Error on pthread_create" << std::endl;
            exit(1);
        };
    }
    singleThread(&contexts[multiThreadLevel - 1]);

    for (int i = 0; i < multiThreadLevel-1; ++i) {
        if (pthread_join(threads[i], nullptr) != 0) {
            std::cerr << "Error on pthread_join" << std::endl;
            exit(1);
        };
    }
//    sem_destroy(&sem);
//    pthread_mutex_destroy(&shuffle_mutex);
//    pthread_mutex_destroy(&reduce_mutex);
    if (pthread_mutex_destroy(&shuffle_mutex) != 0 or pthread_mutex_destroy(&reduce_mutex) != 0) {
        std::cerr << "Error on pthread_mutex_destroy" << std::endl;
        exit(1);
    }
    if (sem_destroy(&sem) != 0) {
        std::cerr << "Error on sem_destroy" << std::endl;
        exit(1);
    }

}


