#include <pthread.h>
#include <cstdio>
#include <atomic>
#include "MapReduceFramework.h"
#include "Barrier.h" // Yuval: TODO: Barrier class needs to be updated to have return value checks for mutex (in case of error)
#include <algorithm> // for std::sort
#include <iostream> // Yuval: TODO: remove after testing
#include <semaphore.h>

typedef std::vector<IntermediateVec> IntermediateVectors;
typedef std::vector<IntermediateVec*> Queue;


bool keyEqual2(K2* a, K2* b){
    return !(*a < *b) && !(*b < *a);
}

struct ThreadContext {
    /** Input from runMapReduceFramework */
    const MapReduceClient* client;
    const InputVec* inputVector;
    OutputVec* outputVec;
    IntermediateVectors* intermediateVectors;
    Queue* shuffleVectors;
    int threadID;
    Barrier* barrier;
    std::atomic<int>* inputVectorIndex;
    sem_t* semaphore;
    pthread_mutex_t* mutex;
    std::atomic<bool>* isShuffleDone;
};


void shufflePhase(const ThreadContext *tc);

void mapPhase(ThreadContext *tc) {
    /** Map phase: Take input from inputVector */
    unsigned long inputSize = tc->inputVector->size();
    int previousIndex = 0;
    while (true) {
        previousIndex = (*tc->inputVectorIndex)++;
        if (previousIndex < inputSize) { // Yuval: TODO: check if the logic here is ok for threads
            const InputPair& pair = (*tc->inputVector)[previousIndex];
            // pair.first = K1, pair.second = V1
            tc->client->map(pair.first, pair.second, tc);
        } else {
            break;
        }
    }
}

void sortPhase(const ThreadContext *tc) {
    IntermediateVec intermediateVec = (*tc->intermediateVectors)[tc->threadID];
    sort(intermediateVec.begin(), intermediateVec.end()); // Hagar: TODO: chnged to pointer - sorts inplace? , Yuval: TODO: check sort key/value
    tc->barrier->barrier();
}

void shufflePhase(const ThreadContext *tc) {
    K2* curr_key = nullptr;
//    std::cout << (*tc->intermediateVectors).size() << std::endl;
//    unsigned long threadNum;
//    threadNum = 1;//(*tc->intermediateVectors).size();

    while (!(*tc->intermediateVectors).empty()) {

        for (auto sortedVecIter = tc->intermediateVectors->begin(); sortedVecIter != tc->intermediateVectors->end(); ) {
            if (sortedVecIter->empty()) {
                sortedVecIter = tc->intermediateVectors->erase(sortedVecIter);
            } else {
                sortedVecIter++;
            }
        }

        curr_key = ((*tc->intermediateVectors)[0])[0].first;

        auto curr_vec = new IntermediateVec();

        for (auto sortedVecIter = tc->intermediateVectors->begin(); sortedVecIter != tc->intermediateVectors->end(); ) {

            for (auto keyIter = sortedVecIter->begin(); keyIter != sortedVecIter->end(); ) {
                if (keyEqual2(keyIter->first, curr_key)) {
                    curr_vec->push_back(*keyIter);
                    keyIter = sortedVecIter->erase(keyIter);
                } else {
                    ++keyIter;
                }
            }
            if (sortedVecIter->empty()) {

                sortedVecIter = tc->intermediateVectors->erase(sortedVecIter);

            } else {
                sortedVecIter++;
            }
        }

        pthread_mutex_lock(tc->mutex);
        (*tc->shuffleVectors).push_back(curr_vec);
        sem_post(tc->semaphore);
        pthread_mutex_unlock(tc->mutex);

    }

//    pthread_mutex_lock(tc->mutex);


//    for (unsigned long i = 0; i < threadNum; ++i) {
//        sem_post(tc->semaphore);
//    }
//    *tc->isShuffleDone = true;

//    pthread_mutex_unlock(tc->mutex);


}

void *singleThread(void *arg) {
    auto* tc = (ThreadContext*) arg;
    mapPhase(tc);

    /** Sort phase: sort this thread's intermediate vector */
    sortPhase(tc);

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

    /** Shuffle phase: only for thread 0 */
    if (tc->threadID == 0) {
        shufflePhase(tc);
        *tc->isShuffleDone = true;
    }


    /** Reduce phase: */
    while (true) {
        int sem_val;

        sem_getvalue(tc->semaphore, &sem_val);
        IntermediateVec* currVecRed = nullptr;

        std::cout << "hi0" << std::endl;

        if (!(*tc->isShuffleDone && sem_val == 0)){

            sem_wait(tc->semaphore);
            pthread_mutex_lock(tc->mutex);

            if (!((*tc->shuffleVectors).empty())){
                currVecRed = (*tc->shuffleVectors).back();
                (*tc->shuffleVectors).pop_back();
            }
            std::cout << "hi1" << std::endl;

            pthread_mutex_unlock(tc->mutex);
            std::cout << "hi2" << std::endl;

            tc->client->reduce(currVecRed, tc);

        } else {
            std::cout << "hi31" << std::endl;

            break;
        }
        std::cout << "hi32" << std::endl;

    }
    std::cout << "hi33" << std::endl;

}


void runMapReduceFramework(const MapReduceClient& client,
                           const InputVec& inputVec, OutputVec& outputVec,
                           int multiThreadLevel) {

    pthread_t threads[multiThreadLevel];
    ThreadContext contexts[multiThreadLevel];
    Barrier barrier(multiThreadLevel);
    IntermediateVectors intermediateVectors(multiThreadLevel);
    Queue shuffleVectors(0);
    std::atomic<int> inputIndex(0);
    sem_t sem;
    std::atomic<bool> isShuffleDone(false);

    if (!sem_init(&sem, 0, 0)){
        //hagar, TODO: Error management
    }

    pthread_mutex_t mutex(PTHREAD_MUTEX_INITIALIZER);


    for (int id = 0; id < multiThreadLevel; ++id) {
        contexts[id] = {&client, &inputVec, &outputVec, &intermediateVectors, &shuffleVectors,
                       id, &barrier, &inputIndex, &sem, &mutex, &isShuffleDone};
    }

    for (int i = 0; i < multiThreadLevel; ++i) {
        pthread_create(threads + i, nullptr, singleThread, contexts + i);
    }

    for (int i = 0; i < multiThreadLevel; ++i) {
        pthread_join(threads[i], nullptr);
    }

}


void emit2 (K2* key, V2* value, void* context) {
    auto* tc = (ThreadContext*) context;
    IntermediatePair pair = IntermediatePair(key, value);
    (*tc->intermediateVectors)[tc->threadID].push_back(pair);
}

void emit3 (K3* key, V3* value, void* context) {
    auto* tc = (ThreadContext*) context;
    OutputPair pair = OutputPair(key, value);

    pthread_mutex_lock(tc->mutex);
    (*tc->outputVec).push_back(pair);
    pthread_mutex_unlock(tc->mutex);
}


