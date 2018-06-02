#include <pthread.h>
#include <cstdio>
#include <atomic>
#include "MapReduceFramework.h"
#include "Barrier.h" // Yuval: TODO: Barrier class needs to be updated to have return value checks for mutex (in case of error)
#include <algorithm> // for std::sort
#include <iostream> // Yuval: TODO: remove after testing
#include <semaphore.h>

typedef std::vector<IntermediateVec> IntermediateVectors;

bool keyEqual2(K2* a, K2* b){
    return !(*a < *b) && !(*b < *a);
}

struct ThreadContext {
    /** Input from runMapReduceFramework */
    const MapReduceClient* client;
    const InputVec* inputVector;
    OutputVec* outputVec;
    IntermediateVectors* intermediateVectors;
    IntermediateVectors* shuffleVectors;
    int threadID;
    Barrier* barrier;
    std::atomic<int>* inputIndex;
    sem_t* semaphore;
    pthread_mutex_t* mutex;
    pthread_cond_t* cv;
    sem_t* writersCount;
};
//
//void shuffleWrite(void *arg){
//    auto* tc = (ThreadContext*) arg;
//    int count;
//    pthread_mutex_lock(tc->mutex);
//    sem_getvalue(tc->writersCount, &count);
//    if (count == 1) {
//        pthread_cond_wait(tc->cv, tc->mutex);
//    } else {
//        count = 0;
//        pthread_cond_broadcast(tc->cv);
//    }
//    pthread_mutex_unlock(tc->mutex);
//}

void *singleThread(void *arg) {
    auto* tc = (ThreadContext*) arg;
    /** Map phase: Take input from inputVector */
    unsigned long inputCount = tc->inputVector->size();
    int previousIndex = 0;
    while (true) {
        previousIndex = (*(tc->inputIndex))++;
        if (previousIndex < inputCount) { // Yuval: TODO: check if the logic here is ok for threads
            InputPair pair = (*tc->inputVector)[previousIndex];
            tc->client->map(pair.first, pair.second, tc);
        } else {
            break;
        }
    }

    /** Sort phase: sort this thread's intermediate vector */
    IntermediateVec intermediateVec = (*tc->intermediateVectors)[tc->threadID];
    std::sort(intermediateVec.begin(), intermediateVec.end());
    tc->barrier->barrier();

    /**
     * Use a vector for the queue (note that it is a vector of vectors), with a semaphore for counting the
number of vectors in it. Whenever a new vector is inserted to the queue we will call sem_post() on
the semaphore to notify the reducing threads that they have pending work. Note that you will also
need a mutex for protecting the access to this queue
     */
    /** Shuffle phase: only for thread 0 */
    if (tc->threadID == 0) {
        while (!(*tc->intermediateVectors).empty()){
            K2 curr_key = *(*tc->intermediateVectors)[0][0].first;
            IntermediateVec curr_vec = IntermediateVec(0);
            for (int i = 0; i < (*tc->intermediateVectors).size(); ++i) {
                int k = 0;
                while (k < (*tc->intermediateVectors)[i].size() && keyEqual2(&curr_key, (*tc->intermediateVectors)[i][k].first)){
                    curr_vec.push_back((*tc->intermediateVectors)[i][k]);
                    k++;
                }
                (*tc->intermediateVectors)[i].erase((*tc->intermediateVectors)[i].begin(),
                                                    (*tc->intermediateVectors)[i].begin() + k);

                if ((*tc->intermediateVectors)[i].empty()){
                    (*tc->intermediateVectors).erase((*tc->intermediateVectors).begin() + i);
                }
            }


//          int val = 0;
            pthread_mutex_lock(tc->mutex);

            (*tc->shuffleVectors).push_back(curr_vec);
            sem_post(tc->semaphore);

            pthread_mutex_unlock(tc->mutex);

        }


//        /** testing Map and Sort phases: */
//        char *c;
//        int *i;
//
//        for (int id = 0; id < tc->intermediateVectors->size(); ++id) {
//            std::cout << "thread " << std::to_string(id) << "'s intermediateVec:" << std::endl;
//            for (const auto pair : (*tc->intermediateVectors)[id]) {
//                std::cout << "(" << pair.first->as_string() << ", " << pair.second->as_string() << ")" << std::endl;
//            }
//            std::cout << std::endl;
//        }
//        /** end of testing section */
    }

    /** Reduce phase: */
    while (true) {
        int sem_val;
        sem_getvalue(tc->semaphore, &sem_val);
        IntermediateVec *currVec = NULL;
        if (!(sem_val == 0 && (*tc->intermediateVectors).empty())){

            sem_wait(tc->semaphore);

            pthread_mutex_lock(tc->mutex);

            currVec = &(*tc->shuffleVectors)[-1];
            (*tc->shuffleVectors).pop_back();

            pthread_mutex_unlock(tc->mutex);
            tc->client->reduce(currVec, tc);


        } else {
            break;
        }
    }

}

void runMapReduceFramework(const MapReduceClient& client,
                           const InputVec& inputVec, OutputVec& outputVec,
                           int multiThreadLevel) {

    pthread_t threads[multiThreadLevel];
    ThreadContext contexts[multiThreadLevel];
    Barrier barrier(multiThreadLevel);
    IntermediateVectors intermediateVectors(multiThreadLevel);
    IntermediateVectors shuffleVectors(0);
    std::atomic<int> inputIndex(0);
    sem_t sem;
    if (!sem_init(&sem, 0, 0)){
        //hagar, TODO: Error management
    }

    pthread_mutex_t mutex(PTHREAD_MUTEX_INITIALIZER);
    pthread_cond_t cv(PTHREAD_COND_INITIALIZER);
    sem_t writersCount;

    if (!sem_init(&writersCount, 0, 1)){
        //hagar, TODO: Error management
    }

    for (int id = 0; id < multiThreadLevel; ++id) {
        contexts[id] = {&client, &inputVec, &outputVec, &intermediateVectors, &shuffleVectors,
                       id, &barrier, &inputIndex, &sem, &mutex, &cv, &writersCount};
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


