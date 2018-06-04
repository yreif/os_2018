#include <pthread.h>
#include <cstdio>
#include <atomic>
#include "MapReduceFramework.h"
#include "Barrier.h" // Yuval: TODO: Barrier class needs to be updated to have return value checks for mutex (in case of error)
#include <algorithm> // for std::sort
#include <iostream> // Yuval: TODO: remove after testing
#include <semaphore.h>

typedef std::vector<IntermediateVec*> IntermediateVectors;

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
    std::atomic<int>* inputVectorIndex;
    sem_t* semaphore;
    pthread_mutex_t* mutex;
//    pthread_cond_t* cv;
//    sem_t* writersCount;
};


void mapPhase(ThreadContext *tc) {
    /** Map phase: Take input from inputVector */
    unsigned long inputSize = tc->inputVector->size();
    int previousIndex = 0;
    while (true) {
        previousIndex = (*tc->inputVectorIndex)++;
        if (previousIndex < inputSize) { // Yuval: TODO: check if the logic here is ok for threads
            InputPair pair = (*tc->inputVector)[previousIndex];
            // pair.first = K1, pair.second = V1
            tc->client->map(pair.first, pair.second, tc);
        } else {
            break;
        }
    }
}


void sortPhase(const ThreadContext *tc) {
    IntermediateVec* intermediateVec = (*tc->intermediateVectors)[tc->threadID];
    sort(intermediateVec->begin(), intermediateVec->end()); // Hagar: TODO: chnged to pointer - sorts inplace? , Yuval: TODO: check sort key/value
    tc->barrier->barrier();
}

void *singleThread(void *arg) {
    auto* tc = (ThreadContext*) arg;
    mapPhase(tc);

    /** Sort phase: sort this thread's intermediate vector */
    sortPhase(tc);

/**    /** Map phase: Take input from inputVector
//    unsigned long inputSize = tc->inputVector->size();
//    int previousIndex = 0;
//    while (true) {
//        previousIndex = (*(tc->inputVectorIndex))++;
//        if (previousIndex < inputSize) { // Yuval: TODO: check if the logic here is ok for threads
//            InputPair pair = (*tc->inputVector)[previousIndex];
//            // pair.first = K1, pair.second = V1
//            tc->client->map(pair.first, pair.second, tc);
//        } else {
//            break;
//        }
//    }
//    /** testing Map and Sort phases:
//    char *c;
//    int *i;
//
//    for (int id = 0; id < tc->intermediateVectors->size(); ++id) {
//        std::cout << "thread " << std::to_string(id) << "'s intermediateVec:" << std::endl;
//        for (const auto pair : (*tc->intermediateVectors)[id]) {
//            std::cout << "(" << pair.first->as_string() << ", " << pair.second->as_string() << ")" << std::endl;
//        }
//        std::cout << std::endl;
//    }
     end of testing section */

    /** Shuffle phase: only for thread 0 */
    if (tc->threadID == 0) {
        K2* curr_key = nullptr;
        while (!(*tc->intermediateVectors).empty()){

            if ((*tc->intermediateVectors).empty()) break;
            if ((*tc->intermediateVectors)[0]->empty()){
                (*tc->intermediateVectors).erase((*tc->intermediateVectors).begin());
            }

            curr_key = ((*tc->intermediateVectors)[0])[0][0].first;
            IntermediateVec* curr_vec = new IntermediateVec();

            for (int i = 0; i < (*tc->intermediateVectors).size(); ++i) {

                int k = 0;
                while (k < (*tc->intermediateVectors)[i]->size()){
                    if (keyEqual2(curr_key, (*((*tc->intermediateVectors)[i]))[k].first)){
                        curr_vec->push_back((*((*tc->intermediateVectors)[i]))[k]);
                    }
                    k++;
                }
                if (k != 0){

                    pthread_mutex_lock(tc->mutex);

                    (*((*tc->intermediateVectors)[i])).erase((*((*tc->intermediateVectors)[i])).begin(),
                                                        (*((*tc->intermediateVectors)[i])).begin() + k);
                    pthread_mutex_unlock(tc->mutex);

                }

            }
            int deleted = 0;
            for (int m = 0; m < (*tc->intermediateVectors).size(); ++m) {

                if ((*((*tc->intermediateVectors)[m - deleted])).empty()) {

                    pthread_mutex_lock(tc->mutex);

                    (*tc->intermediateVectors).erase((*tc->intermediateVectors).begin() + m - deleted);
                    deleted ++;
                    pthread_mutex_unlock(tc->mutex);

                }
            }


            pthread_mutex_lock(tc->mutex);
            (*tc->shuffleVectors).push_back(curr_vec);
            pthread_mutex_unlock(tc->mutex);

            sem_post(tc->semaphore);

        }

        /** testing Map and Sort phases:
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
        IntermediateVec* currVecRed = nullptr;

        if (!(sem_val == 0 && (*tc->intermediateVectors).empty())){

            sem_wait(tc->semaphore);

            pthread_mutex_lock(tc->mutex);

            currVecRed = (*tc->shuffleVectors).back();
            (*tc->shuffleVectors).pop_back();

            tc->client->reduce(currVecRed, tc);

            pthread_mutex_unlock(tc->mutex);

        } else {
            break;
        }

    }


    /** Map phase: Take input from inputVector
//    unsigned long inputSize = tc->inputVector->size();
//    int previousIndex = 0;
//    while (true) {
//        previousIndex = (*(tc->inputVectorIndex))++;
//        if (previousIndex < inputSize) { // Yuval: TODO: check if the logic here is ok for threads
//            InputPair pair = (*tc->inputVector)[previousIndex];
//            // pair.first = K1, pair.second = V1
//            tc->client->map(pair.first, pair.second, tc);
//        } else {
//            break;
//        }
//    } */
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
//    pthread_cond_t cv(PTHREAD_COND_INITIALIZER);
//    sem_t writersCount;

//    if (!sem_init(&writersCount, 0, 1)){
//        //hagar, TODO: Error management
//    }

    for (int id = 0; id < multiThreadLevel; ++id) {
        intermediateVectors[id] = new IntermediateVec();
        contexts[id] = {&client, &inputVec, &outputVec, &intermediateVectors, &shuffleVectors,
                       id, &barrier, &inputIndex, &sem, &mutex};
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
    (*tc->intermediateVectors)[tc->threadID]->push_back(pair);
}

void emit3 (K3* key, V3* value, void* context) {
    auto* tc = (ThreadContext*) context;
    OutputPair pair = OutputPair(key, value);

    pthread_mutex_lock(tc->mutex);

    (*tc->outputVec).push_back(pair);

    pthread_mutex_unlock(tc->mutex);
}


