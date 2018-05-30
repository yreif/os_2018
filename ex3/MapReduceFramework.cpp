#include <pthread.h>
#include <cstdio>
#include <atomic>
#include "MapReduceFramework.h"
#include "Barrier.h" // Yuval: TODO: Barrier class needs to be updated to have return value checks for mutex (in case of error)
#include <algorithm> // for std::sort
#include <iostream> // Yuval: TODO: remove after testing

typedef std::vector<IntermediateVec> IntermediateVectors;

struct ThreadContext {
    /** Input from runMapReduceFramework */
    const MapReduceClient* client;
    const InputVec* inputVector;
    OutputVec* outputVec;
    IntermediateVectors* intermediateVectors;
    int threadID;
    Barrier* barrier;
    std::atomic<int>* inputIndex;


};

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

    /** Shuffle phase: only for thread 0 */
    if (tc->threadID == 0) {
        /** testing Map and Sort phases: */
        char *c;
        int *i;

        for (int id = 0; id < tc->intermediateVectors->size(); ++id) {
            std::cout << "thread " << std::to_string(id) << "'s intermediateVec:" << std::endl;
            for (const auto pair : (*tc->intermediateVectors)[id]) {
                std::cout << "(" << pair.first->as_string() << ", " << pair.second->as_string() << ")" << std::endl;
            }
            std::cout << std::endl;
        }
        /** end of testing section */
    }

    /** Reduce phase: */

}

void runMapReduceFramework(const MapReduceClient& client,
                           const InputVec& inputVec, OutputVec& outputVec,
                           int multiThreadLevel) {

    pthread_t threads[multiThreadLevel];
    ThreadContext contexts[multiThreadLevel];
    Barrier barrier(multiThreadLevel);
    IntermediateVectors intermediateVectors(multiThreadLevel);
    std::atomic<int> inputIndex(0);


    for (int id = 0; id < multiThreadLevel; ++id) {
        contexts[id] = {&client, &inputVec, &outputVec, &intermediateVectors,
                       id, &barrier, &inputIndex};
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

}


