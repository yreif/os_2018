#include "VirtualMemory.h"
#include "PhysicalMemory.h"

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



inline bool notInPath(uint64_t frameIndex, const pathArray& path) {
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
        if (static_cast<uint64_t>(curr_val) == frameIndex)
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
                      uint64_t currPageNum, const pathArray& path, ChooseFrameHelper& helper) {
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
    if (currDepth == 0) currPageNum = currPageNum << getRootWidth();
    else currPageNum = currPageNum << OFFSET_WIDTH;

    for (uint64_t i = 0; i < PAGE_SIZE; ++i)
    {
        PMread(currFrameIndex * PAGE_SIZE + i, &curr_val);
        nextFrameIndex =  static_cast<uint64_t>(curr_val);
        if (curr_val != 0)
        {
            nextFrameIndex = searchFrames(currDepth+1, nextFrameIndex, currFrameIndex,
                                          currPageNum + i, path, helper);
            if (nextFrameIndex != 0) return nextFrameIndex;
        }
    }
    return 0;
}

uint64_t chooseFrame(const pathArray& path, uint64_t desiredPageNumber) {
    ChooseFrameHelper helper = {0};
    helper.desiredPageNumber = desiredPageNumber;
    /** 1st priority - A frame containing an empty table */
    uint64_t chosenFrame = searchFrames(0, 0, 0, 0, path, helper);
    if (chosenFrame != 0) return chosenFrame;

    /** 2rd priority - An unused frame */
    if (helper.maxFrameIndex + 1 < NUM_FRAMES)
    {
        clearTable(helper.maxFrameIndex + 1);
        return helper.maxFrameIndex + 1;
    }
    /** 3rd priority - choose frame by cyclical distance */
    unlinkFrame(helper.maxCyclicDistFrame, helper.maxCyclicDistParent);
    PMevict(helper.maxCyclicDistFrame, helper.maxCyclicDistPageNum);
    clearTable(helper.maxCyclicDistFrame); //- only necessary in tables, since we're going to restore a page here
    return helper.maxCyclicDistFrame;
}

uint64_t getPhysicalAddress(uint64_t virtualAddress) {
    uint64_t offset = getOffset(virtualAddress);
    uint64_t pageNum = getPageNum(virtualAddress);
    uint64_t currAddress = 0;
    uint64_t nextAddress = 0;
    uint64_t chosenFrame;
    pathArray path = {0};

    // first translation - root
    currAddress = extractBits(pageNum, getRootWidth(), (TABLES_DEPTH - 1) * OFFSET_WIDTH);
    PMread(currAddress, reinterpret_cast<word_t *>(&nextAddress));
    if (nextAddress == 0)
    {
        chosenFrame = chooseFrame(path, pageNum);
        PMwrite(currAddress, chosenFrame);
        nextAddress = chosenFrame;
        /** In actual pages: restore the page we are looking for to chosenFrame  */
        if (TABLES_DEPTH == 1) PMrestore(chosenFrame ,pageNum); // we've arrived at a page
    }
    path[0] = nextAddress;

    // translate the rest
    for (uint64_t i = 1; i < TABLES_DEPTH; ++i)
    {
        currAddress = (nextAddress * PAGE_SIZE)
                      + extractBits(pageNum, OFFSET_WIDTH, (TABLES_DEPTH - i - 1) * OFFSET_WIDTH);
        PMread(currAddress, reinterpret_cast<word_t *>(&nextAddress));
        if (nextAddress == 0)
        {
            chosenFrame = chooseFrame(path, pageNum);
            /** In actual pages: restore the page we are looking for to chosenFrame  */
            if (i == TABLES_DEPTH - 1) // we've arrived at a page
            {
                PMrestore(chosenFrame, pageNum);
            }
            PMwrite(currAddress, chosenFrame);
            nextAddress = chosenFrame;
        }
        path[i] = nextAddress;
    }
    return nextAddress * PAGE_SIZE + offset;
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
