#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>
#include <iostream> // TODO: remove
#include <bitset> // TODO: remove

void printVirtual(uint64_t address) {
    std::bitset<VIRTUAL_ADDRESS_WIDTH> printVirtual = address;
    std::cout << printVirtual << std::endl;
}

typedef uint64_t pathArray[TABLES_DEPTH];

typedef struct ChooseFrameHelper {
    uint64_t maxFrameIndex;
    uint64_t desiredPageNumber;
    uint64_t maxCyclicDistPageNum;
    uint64_t maxCyclicDist;
    uint64_t maxCyclicDistFrame;
    uint64_t maxCyclicDistParent;
} ChooseFrameHelper;

uint64_t calcCyclicDist(uint64_t pageNumber, uint64_t otherPageNumber){
    uint64_t v = 0;
    if (pageNumber > otherPageNumber) v = pageNumber - otherPageNumber;
    else v = otherPageNumber - pageNumber;

    if ((NUM_PAGES - v) < v) return (NUM_PAGES - v);
    return v;
}

inline uint64_t getRootWidth() {
    if ((VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH) == 0) return OFFSET_WIDTH;
    return (VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH);
}

inline uint64_t extractBits(uint64_t originalNum, uint64_t numBits, uint64_t startingPosition) // startingPosition is from the right
{
    return (((1 << numBits) - 1) & (originalNum >> (startingPosition)));
}

inline uint64_t getOffset(uint64_t virtualAddress){
    return extractBits(virtualAddress, OFFSET_WIDTH, 0);
}

inline uint64_t getPageNum(uint64_t virtualAddress) {
    return virtualAddress >> OFFSET_WIDTH;
}



inline bool notInPath(uint64_t frameIndex, const pathArray& path) { // tOdO Yuval: think this through again. I'm ignoring frame 0 in the path
    if (frameIndex == 0) return true;
    for (auto pathFrame : path)
    {
        if (frameIndex == pathFrame)
        {
            return false;
        }
        if (pathFrame == 0)
        {
            return true;
        }
    }
    return true;
}

void clearTable(uint64_t frameIndex) {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
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

bool isFrameEmpty(uint64_t frameIndex){
    word_t curr_val=0;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMread(frameIndex * PAGE_SIZE + i, &curr_val);
        if (curr_val != 0) return false;
    }
    return true;
}

uint64_t searchFrames(uint64_t currDepth, uint64_t currFrameIndex, uint64_t currParent,
                      uint64_t currPageNum, const pathArray& path, ChooseFrameHelper& helper, int callNumber) {
//    std::cout << "i'm call #" << callNumber << " and i'm at frame " << currFrameIndex << " while searching for page " << helper.desiredPageNumber << ", my currPageNum is " << currPageNum << std::endl;
    if (isFrameEmpty(currFrameIndex) && notInPath(currFrameIndex, path))

    {
        unlinkFrame(currFrameIndex, currParent);
        return currFrameIndex;
    }
    if (currFrameIndex > helper.maxFrameIndex)
    {
        helper.maxFrameIndex = currFrameIndex;
    }

    if (currDepth == TABLES_DEPTH) // we've arrived at a page (a leaf)
    {
//        std::cout << "i'm call #" << callNumber << " and i've reached page " << currPageNum << " which is located at frame " << currFrameIndex << " while searching for page " << helper.desiredPageNumber << std::endl;
        uint64_t currCyclicDist = calcCyclicDist(currPageNum, helper.desiredPageNumber);
        if (currCyclicDist > helper.maxCyclicDist)
        {
            helper.maxCyclicDist = currCyclicDist;
            helper.maxCyclicDistPageNum = currPageNum;
            helper.maxCyclicDistFrame = currFrameIndex;
            helper.maxCyclicDistParent = currParent;
        }
        return 0;
    }

    // otherwise, continue traversing tree:
    word_t curr_val = 0;
    uint64_t nextFrameIndex = 0;
    uint64_t nextPageNum;
    currPageNum = currPageNum << OFFSET_WIDTH; /**Calculating the (virtual) page backwards each time */
    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMread(currFrameIndex * PAGE_SIZE + i, &curr_val);
        nextFrameIndex =  static_cast<uint64_t>(curr_val);
        if (curr_val != 0)
        {
            nextFrameIndex = searchFrames(currDepth+1, nextFrameIndex, currFrameIndex,
                                          currPageNum + i, path, helper, callNumber);
            if (nextFrameIndex != 0) return nextFrameIndex;


//            if (currDepth < (TABLES_DEPTH - 1))
//            {
//                nextFrameIndex = searchFrames(currDepth+1, nextFrameIndex, currFrameIndex,
//                                              currPageNum + i, path, helper, callNumber);
//            }
//            else
//            {
//                nextFrameIndex = searchFrames(currDepth+1, nextFrameIndex, currFrameIndex,
//                                              currPageNum + i, path, helper, callNumber);
//            }


        }
    }
}

uint64_t chooseFrame(const pathArray& path, uint64_t desiredPageNumber, int callNumber) {
    ChooseFrameHelper helper = {0};
    helper.desiredPageNumber = desiredPageNumber;
    /** 1st priority - A frame containing an empty table */
    uint64_t chosenFrame = searchFrames(0, 0, 0, 0, path, helper, callNumber);
    if (chosenFrame != 0) return chosenFrame;

    /** 2rd priority - An unused frame */
    if (helper.maxFrameIndex + 1 < NUM_FRAMES)
    {
//        std::cout << "creating new frame at max index " << helper.maxFrameIndex + 1 << " on my way to page " << desiredPageNumber << std::endl;
        clearTable(helper.maxFrameIndex + 1);
        return helper.maxFrameIndex + 1;
    }
    /** 3rd priority - choose frame by cyclical distance */
    unlinkFrame(helper.maxCyclicDistFrame, helper.maxCyclicDistParent);
//    std::cout << "evicting page " << helper.maxCyclicDistPageNum << " from frame " << helper.maxCyclicDistFrame << std::endl;
    if (helper.maxCyclicDistPageNum == 20 && (helper.maxCyclicDistFrame == 12 || helper.maxCyclicDistFrame == 4))
    {
        int a = 0;
    }
    PMevict(helper.maxCyclicDistFrame, helper.maxCyclicDistPageNum);
    clearTable(helper.maxCyclicDistFrame); //- only necessary in tables, since we're going to restore a page here
    return helper.maxCyclicDistFrame;
}


//void createTable(uint64_t frameIndex, uint64_t parentIndex) {
//    /** first clear the table */
//    clearTable(frameIndex);
//    /** unlink this frame from the table above it */
//    unlinkFrame(frameIndex, parentIndex);
//}
////    {
////        /** 3rd priority - choose frame by cyclical distance */
////        /* No frames containing an empty table or unused frames were found - so choose frame by cyclical distance */
////        PMevict(maxCyclicDistFrame, maxCyclicDistPage);
////        unlinkFrame(maxCyclicDistFrame, maxCyclicDistParent);
////        clearTable(maxCyclicDistFrame);
////        return maxCyclicDistFrame;
////    }
//
//
////uint64_t findEmptyTable(uint64_t curr_depth, uint64_t frameIndex, uint64_t &curr_parentFrame,
////                        uint64_t &maxFrameI,uint64_t avoid){
////    /** Return index 0 if not found, meaning we reached a frame not used for tables.
////     *0 since we know frame 0 is locked and used by the first level table,
////     *and since we use unsigned int -1 is not an option. */
////    if (curr_depth == TABLES_DEPTH + 1) return 0;
//////    if (curr_depth == TABLES_DEPTH - 1) return 0; //TODO: figure out the depth part, when to stop?
////
////    if (isFrameEmpty(frameIndex)) /** If the current frame is empty, then it's a free table */
////    {
////        unlinkFrame(frameIndex, curr_parentFrame); // ToDo - Yuval: is it ok if we do this inside this function instead of outside?
////        return frameIndex;
////    }
////
////
////
//////    if (curr_depth+1 == TABLES_DEPTH) return 0;
////// /*From below on we assume there is extra level of frames and we don't reach vars *//TODO: not sure if needed
////
////    word_t curr_val = 0;
////    uint64_t new_frameIndex = 0;
////    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
////    {
////        PMread(frameIndex * PAGE_SIZE + i, &curr_val);
////        new_frameIndex =  static_cast<uint64_t>(curr_val);
////        if (curr_val != 0)
////        {
////            /**Keeping max index of frame referenced in a table */
////            if (new_frameIndex > maxFrameI) maxFrameI = new_frameIndex;
////
////            if (isFrameEmpty(new_frameIndex) && (avoid != new_frameIndex))
////            { /*If avoid[curr_depth]*/
////                /** New frame pointed from the table in frame index is empty, and we can now return
////                 * This is the only place where curr_parentFrame needs to be updated*/
////                curr_parentFrame = frameIndex;
////                return new_frameIndex;
////
////            }
////            else if ((new_frameIndex=findEmptyTable(curr_depth+1, new_frameIndex, frameIndex, maxFrameI, avoid)) != 0)
////            {
////                /**one of the tables in the level's below are empty, and they update the parent if needed*/
////                return new_frameIndex;
////            }
////        }
////    }
////}
////
////void find_frameToEvict(uint64_t pageNumber, uint64_t& avoid[TABLES_DEPTH], uint64_t curr_page, uint64_t parent, uint64_t &maxparent,
////                          uint64_t &max_page, uint64_t &max_cyclic_dist, uint64_t &max_phys_fr, uint64_t curr_depth){
////    /**Recursively find the page (not in use by ours) with the maximal cyclic distance (kept by reference */
////    if (curr_depth == TABLES_DEPTH) { /**Here it is already the (virtual) page */
////        for (int j = 0; j < TABLES_DEPTH; ++j) {
////            if (avoid[j] == curr_page) return;
////        }
////        if (calcCyclicDist(pageNumber, curr_page) > max_cyclic_dist){
////            max_page = curr_page;
////            max_cyclic_dist = calcCyclicDist(pageNumber, curr_page);
////            maxparent = parent;
////        }
////    }
//////    uint64_t leveled_parent = curr_page >> (uint64_t(log2(PAGE_SIZE)) * (TABLES_DEPTH - curr_depth)); TODO: probably not needed
////
////    for (uint64_t i = 0; i < PAGE_SIZE; ++i) { /**For each entry at the page table we check the cyclic distance of it. */
////        word_t curr_val = 0;
////        uint64_t curr_page_num = curr_page;
////        PMread(curr_page * PAGE_SIZE + i, &curr_val);
//////        uint64_t curr_phys = static_cast<uint64_t>(curr_val);
////
////        if (curr_val != 0){
//            curr_page_num = curr_page_num << uint64_t(log2(PAGE_SIZE)); /**Calculating the (virtual) page backwards each time */
//            curr_page_num += i;
////            find_frameToEvict(pageNumber, avoid, curr_page_num, curr_page, maxparent, max_page, max_cyclic_dist,
////                              reinterpret_cast<uint64_t &>(curr_val), curr_depth + 1);
////        }
////    }
////}
////
////
////uint64_t frameEvicter(uint64_t pageNumber, uint64_t& avoid[TABLES_DEPTH]){
////    /**For recursive call, and initializes evicted frame to zero and removes the reference to it */
////    uint64_t parent = 0; uint64_t max_page = 0; uint64_t max_cyclic_dist = 0; uint64_t max_phys_fr=0;
////    find_frameToEvict(pageNumber, avoid, 0, 0,parent, max_page, max_cyclic_dist, max_phys_fr, 0);
////
////    if (max_page == 0 || max_cyclic_dist == 0){
////        //TODO: ERROR, no one to evict - weird
////    }
////    PMevict(max_phys_fr, max_page);  /**Physical memory eviction TODO: NOT SURE if physical mem is accurate */
////
////    /**Cleaning of the frame and the reference to it */
////    createTable(max_phys_fr, parent);
////
////    return max_page;
////}
////
////uint64_t findFreeFrame(uint64_t pageNumber, uint64_t avoid[TABLES_DEPTH], uint64_t curr_depth){
////
////    if (isFrameEmpty(0)){
////        return 1;
////    }/*Assuming the first table isn't empty */
////
////    uint64_t parentFrame = 0;
////    uint64_t maxFrameI = 0; /**The maximum index found referenced by a table */
////
////    uint64_t frameIndex = findEmptyTable(0, 0, parentFrame, maxFrameI, avoid[curr_depth]); /**Try to find an empty table */
////    if(frameIndex != 0){ /*We found an empty one */
////        return frameIndex;
////    }
////    if(maxFrameI + 1 == 0) std::cout<<"overflow"<<std::endl; //TODO: unsigned int overflows to 0
////
////    if(maxFrameI + 1 < NUM_FRAMES) return maxFrameI + 1; /**According to the pdf - since we choose frames by order */
////    return frameEvicter(pageNumber, avoid); /**Evicting policy from the pdf */
////}

uint64_t getPhysicalAddress(uint64_t virtualAddress) {
    int callNumber = 0;
    uint64_t offset = getOffset(virtualAddress);
    uint64_t pageNum = getPageNum(virtualAddress);
    uint64_t currAddress = 0;
    uint64_t nextAddress = 0;
    uint64_t chosenFrame;
    static uint64_t rootWidth = getRootWidth();
    pathArray path = {0};
    word_t curr_val;

//    /* testing prints */
//    std::cout << "root width: " << rootWidth << std::endl;
//    printVirtual(virtualAddress);
//    printVirtual(offset);
//    printVirtual(pageNum);
//    printVirtual(nextAddress);
//    /* end of testing prints */

    // first translation - root
    currAddress = extractBits(pageNum, rootWidth, (TABLES_DEPTH - 1) * OFFSET_WIDTH);
    PMread(currAddress, reinterpret_cast<word_t *>(&nextAddress));
    if (nextAddress == 0)
    {
        chosenFrame = chooseFrame(path, pageNum, callNumber++);
        PMwrite(currAddress, chosenFrame);
        nextAddress = chosenFrame;
        /** In actual pages: restore the page we are looking for to chosenFrame  */
        if (TABLES_DEPTH == 1) PMrestore(chosenFrame ,pageNum); // we've arrived at a page
    }
    path[0] = nextAddress;

    // rest of tree
    for (uint64_t i = 1; i < TABLES_DEPTH; ++i)
    {
        currAddress = (nextAddress * PAGE_SIZE)
                      + extractBits(pageNum, OFFSET_WIDTH, (TABLES_DEPTH - i - 1) * OFFSET_WIDTH);
        PMread(currAddress, reinterpret_cast<word_t *>(&nextAddress));
        if (nextAddress == 0)
        {
            chosenFrame = chooseFrame(path, pageNum, callNumber++);
            /** In actual pages: restore the page we are looking for to chosenFrame  */
            if (i == TABLES_DEPTH - 1) // we've arrived at a page
            {
//                std::cout << "restoring page " << pageNum << " to frame " << chosenFrame << std::endl;
                PMrestore(chosenFrame, pageNum);
            }
            PMwrite(currAddress, chosenFrame);
            nextAddress = chosenFrame;
        }
        path[i] = nextAddress;
    }
    return nextAddress * PAGE_SIZE + offset;


//    for (int i = 0; i < TABLES_DEPTH -1; ++i) {
//        curr_offset = virtualAddress >> (TABLES_DEPTH - i);
//        PMread(currAddress + curr_offset, reinterpret_cast<word_t *>(nextAddress));
//        if (nextAddress == 0) {
//
//        }
//        currAddress = nextAddress;
//    }
}

void VMinitialize() {
    clearTable(0);
}

int VMread(uint64_t virtualAddress, word_t* value) {
    if ((virtualAddress >> VIRTUAL_ADDRESS_WIDTH) != 0) return 0;
    PMread(getPhysicalAddress(virtualAddress), value);
    return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value) {
    if ((virtualAddress >> VIRTUAL_ADDRESS_WIDTH) != 0) return 0;
    PMwrite(getPhysicalAddress(virtualAddress), value);
    return 1;
}
