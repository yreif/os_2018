#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>

typedef struct ChooseFrameHelper {
    uint64_t maxFrameIndex;
    uint64_t maxFrameParent;
    uint64_t maxCyclicDistFrame;
    uint64_t maxCyclicDistParent;
    uint64_t maxCyclicDistPageNumber;
    uint64_t desiredPageNumber;
} ChooseFrameHelper;

void clearTable(uint64_t frameIndex) {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

inline uint64_t getOffset(uint64_t virtualAddress){
    return (1LL << uint64_t((log2(VIRTUAL_MEMORY_SIZE) - log2(PAGE_SIZE)))) & virtualAddress;
//    return virtualAddress % uint64_t(log2(VIRTUAL_MEMORY_SIZE) - log2(PAGE_SIZE)); // TODO: remove this
}

inline uint64_t getPageNum(uint64_t virtualAddress) {
    return virtualAddress >> OFFSET_WIDTH;
}

bool isFrameEmpty(uint64_t frameIndex){
    word_t curr_val=0;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMread(frameIndex * PAGE_SIZE + i, &curr_val);
        if (curr_val != 0) return false;
    }
    return true;
}

void unlinkFrame(uint64_t frameIndex, uint64_t parentTableIndex) {
    word_t curr_val = 0;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMread(parentTableIndex * PAGE_SIZE + i, &curr_val);
        if (curr_val == frameIndex)
        {
            PMwrite(parentTableIndex * PAGE_SIZE + i, 0);
            return;
        }
    }
}

void createTable(uint64_t frameIndex, uint64_t parentIndex) {
    /** first clear the table */
    clearTable(frameIndex);
    /** unlink this frame from the table above it */
    unlinkFrame(frameIndex, parentIndex);
//    word_t curr_val=0;
//    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
//    {
//        PMread(parentIndex * PAGE_SIZE + i, &curr_val);
//        if (curr_val == frameI)
//        {
//            PMwrite(parentIndex * PAGE_SIZE + i, 0);
//            return;
//        }
//    }
}

uint64_t calcCyclicDist(uint64_t pageNumber, uint64_t otherPageNumber){
    uint64_t absl=0;
    if (pageNumber > otherPageNumber) absl = pageNumber - otherPageNumber;
    else absl = otherPageNumber - pageNumber;

    uint64_t v = abs(pageNumber - otherPageNumber);

    if ((NUM_PAGES - absl) < absl) return (NUM_PAGES - absl);
    return absl;
}



uint64_t searchFrames(uint64_t currDepth, uint64_t currFrameIndex, uint64_t currParent,
                     uint64_t avoid, uint64_t currPageNum, ChooseFrameHelper& helper) {
    if (isFrameEmpty(currFrameIndex) && (avoid != currFrameIndex))
    {
        unlinkFrame(currFrameIndex, currParent);
        return currFrameIndex;
    }
    if (currFrameIndex > helper.maxFrameIndex)
    {
        helper.maxFrameIndex = currFrameIndex;
        helper.maxFrameParent = currParent;
    }

    if (currDepth == TABLES_DEPTH) // we've arrived at a page (a leaf)
    {

        return 0;
    }

    // otherwise, continue traversing tree:
    word_t curr_val = 0;
    uint64_t nextFrameIndex = 0;
    bool nextIsPage = false;
    if (currDepth == TABLES_DEPTH - 1) nextIsPage = true;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMread(currFrameIndex * PAGE_SIZE + i, &curr_val);
        nextFrameIndex =  static_cast<uint64_t>(curr_val);
        if (curr_val != 0)
        {
            nextFrameIndex = searchFrames(currDepth+1, nextFrameIndex, currFrameIndex,
                                          avoid, currPageNum+i, helper);
            if (nextFrameIndex != 0) return nextFrameIndex;
        }
    }




    //    {
    //        /** 3rd priority - choose frame by cyclical distance */
    //        /* No frames containing an empty table or unused frames were found - so choose frame by cyclical distance */
    //        PMevict(maxCyclicDistFrame, maxCyclicDistPage);
    //        unlinkFrame(maxCyclicDistFrame, maxCyclicDistParent);
    //        clearTable(maxCyclicDistFrame);
    //        return maxCyclicDistFrame;
    //    }

}


uint64_t chooseFrame(uint64_t avoid) {
    ChooseFrameHelper helper;
    /** 1st priority - A frame containing an empty table */
    uint64_t chosenFrame = searchFrames(0, 0, 0, avoid, helper);
    if (chosenFrame != 0) return chosenFrame;

    /** 2rd priority - An unused frame */


    /** 3rd priority - choose frame by cyclical distance */


}


inline bool reachedMaxDepth(uint64_t depth) {
    return depth == TABLES_DEPTH + 1;
}

uint64_t findEmptyTable(uint64_t curr_depth, uint64_t frameIndex, uint64_t &curr_parentFrame,
                        uint64_t &maxFrameI,uint64_t avoid){
    /** Return index 0 if not found, meaning we reached a frame not used for tables.
     *0 since we know frame 0 is locked and used by the first level table,
     *and since we use unsigned int -1 is not an option. */
    if (reachedMaxDepth(curr_depth)) return 0;
//    if (curr_depth == TABLES_DEPTH - 1) return 0; //TODO: figure out the depth part, when to stop?

    if (isFrameEmpty(frameIndex)) /** If the current frame is empty, then it's a free table */
    {
        unlinkFrame(frameIndex, curr_parentFrame); // ToDo - Yuval: is it ok if we do this inside this function instead of outside?
        return frameIndex;
    }



//    if (curr_depth+1 == TABLES_DEPTH) return 0;
// /*From below on we assume there is extra level of frames and we don't reach vars *//TODO: not sure if needed

    word_t curr_val = 0;
    uint64_t new_frameIndex = 0;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMread(frameIndex * PAGE_SIZE + i, &curr_val);
        new_frameIndex =  static_cast<uint64_t>(curr_val);
        if (curr_val != 0)
        {
            /**Keeping max index of frame referenced in a table */
            if (new_frameIndex > maxFrameI) maxFrameI = new_frameIndex;

            if (isFrameEmpty(new_frameIndex) && (avoid != new_frameIndex))
            { /*If avoid[curr_depth]*/
                /** New frame pointed from the table in frame index is empty, and we can now return
                 * This is the only place where curr_parentFrame needs to be updated*/
                curr_parentFrame = frameIndex;
                return new_frameIndex;

            }
            else if ((new_frameIndex=findEmptyTable(curr_depth+1, new_frameIndex, frameIndex, maxFrameI, avoid)) != 0)
            {
                /**one of the tables in the level's below are empty, and they update the parent if needed*/
                return new_frameIndex;
            }
        }
    }
}

void find_frameToEvict(uint64_t pageNumber, uint64_t& avoid[TABLES_DEPTH], uint64_t curr_page, uint64_t parent, uint64_t &maxparent,
                          uint64_t &max_page, uint64_t &max_cyclic_dist, uint64_t &max_phys_fr, uint64_t curr_depth){
    /**Recursively find the page (not in use by ours) with the maximal cyclic distance (kept by reference */
    if (curr_depth == TABLES_DEPTH) { /**Here it is already the (virtual) page */
        for (int j = 0; j < TABLES_DEPTH; ++j) {
            if (avoid[j] == curr_page) return;
        }
        if (calcCyclicDist(pageNumber, curr_page) > max_cyclic_dist){
            max_page = curr_page;
            max_cyclic_dist = calcCyclicDist(pageNumber, curr_page);
            maxparent = parent;
        }
    }
//    uint64_t leveled_parent = curr_page >> (uint64_t(log2(PAGE_SIZE)) * (TABLES_DEPTH - curr_depth)); TODO: probably not needed

    for (uint64_t i = 0; i < PAGE_SIZE; ++i) { /**For each entry at the page table we check the cyclic distance of it. */
        word_t curr_val = 0;
        uint64_t curr_page_num = curr_page;
        PMread(curr_page * PAGE_SIZE + i, &curr_val);
//        uint64_t curr_phys = static_cast<uint64_t>(curr_val);

        if (curr_val != 0){
            curr_page_num = curr_page_num << uint64_t(log2(PAGE_SIZE)); /**Calculating the (virtual) page backwards each time */
            curr_page_num += i;
            find_frameToEvict(pageNumber, avoid, curr_page_num, curr_page, maxparent, max_page, max_cyclic_dist,
                              reinterpret_cast<uint64_t &>(curr_val), curr_depth + 1);
        }
    }
}


uint64_t frameEvicter(uint64_t pageNumber, uint64_t& avoid[TABLES_DEPTH]){
    /**For recursive call, and initializes evicted frame to zero and removes the reference to it */
    uint64_t parent = 0; uint64_t max_page = 0; uint64_t max_cyclic_dist = 0; uint64_t max_phys_fr=0;
    find_frameToEvict(pageNumber, avoid, 0, 0,parent, max_page, max_cyclic_dist, max_phys_fr, 0);

    if (max_page == 0 || max_cyclic_dist == 0){
        //TODO: ERROR, no one to evict - weird
    }
    PMevict(max_phys_fr, max_page);  /**Physical memory eviction TODO: NOT SURE if physical mem is accurate */

    /**Cleaning of the frame and the reference to it */
    createTable(max_phys_fr, parent);

    return max_page;
}

uint64_t findFreeFrame(uint64_t pageNumber, uint64_t avoid[TABLES_DEPTH], uint64_t curr_depth){

    if (isFrameEmpty(0)){
        return 1;
    }/*Assuming the first table isn't empty */

    uint64_t parentFrame = 0;
    uint64_t maxFrameI = 0; /**The maximum index found referenced by a table */

    uint64_t frameIndex = findEmptyTable(0, 0, parentFrame, maxFrameI, avoid[curr_depth]); /**Try to find an empty table */
    if(frameIndex != 0){ /*We found an empty one */
        return frameIndex;
    }
    if(maxFrameI + 1 == 0) std::cout<<"overflow"<<std::endl; //TODO: unsigned int overflows to 0

    if(maxFrameI + 1 < NUM_FRAMES) return maxFrameI + 1; /**According to the pdf - since we choose frames by order */
    return frameEvicter(pageNumber, avoid); /**Evicting policy from the pdf */
}

uint64_t findPysicalAdd(uint64_t virtualAddress){ /** this is the main func to use for read\write?  TODO: not finished */
    uint64_t off_set = getOffset(virtualAddress);
    uint64_t page_num = getPageNum(virtualAddress);
    uint64_t curr_add = 0;
    uint64_t next_add;
    uint64_t curr_offset;
    word_t curr_val;
    for (int i = 0; i < TABLES_DEPTH -1; ++i) {
        curr_offset = virtualAddress >> (TABLES_DEPTH - i);
        PMread(curr_add + curr_offset, reinterpret_cast<word_t *>(next_add));
        if (next_add == 0) {

        }
        curr_add = next_add;
    }

}


void VMinitialize() {
    clearTable(0);
}


int VMread(uint64_t virtualAddress, word_t* value) {
    return 1;
}


int VMwrite(uint64_t virtualAddress, word_t value) {
    return 1;
}
