#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>
#include <iostream>


void clearTable(uint64_t frameIndex) {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

uint64_t getOffset(uint64_t virtualAddress){
    return virtualAddress % uint64_t(log2(VIRTUAL_MEMORY_SIZE) - log2(PAGE_SIZE));
}

uint64_t getPageNum(uint64_t virtualAddress){
    return virtualAddress >> OFFSET_WIDTH;
}

bool isFrameEmpty(uint64_t frameIndex){ /**Simple check according to the given clear table */
    word_t curr_val=0;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMread(frameIndex * PAGE_SIZE + i, &curr_val);
        if (curr_val != 0) return false;
    }
    return true;
}

void createTable(uint64_t frameI, uint64_t parentI){ /**More like cleaning the table */
    clearTable(frameI);
    word_t curr_val=0;

    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMread(parentI * PAGE_SIZE + i, &curr_val);
        if (curr_val == frameI){
            PMwrite(parentI * PAGE_SIZE + i, 0);
            return;
        }
    }
}

uint64_t findEmptyTable(uint64_t curr_depth, uint64_t frameIndex, uint64_t &curr_parentFrame, uint64_t &maxFrameI, uint64_t avoid){
    /**Return index 0 if not found, meaning we reached a frame not used for tables.
     *0 since we know frame 0 is locked and used by the first level table,
     *and since we use unsigned int -1 is not an option. */
    if (curr_depth == TABLES_DEPTH - 1) return 0; //TODO: figure out the depth part, when to stop?

    if (isFrameEmpty(frameIndex)) return frameIndex; /*If the current frame is empty, then it's a free table */

//    if (curr_depth+1 == TABLES_DEPTH) return 0; /*From below on we assume there is extra level of frames and we don't reach vars *//TODO: not sure if needed

    word_t curr_val = 0;
    uint64_t new_FrameIndex = 0;

    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMread(frameIndex * PAGE_SIZE + i, &curr_val);
        new_FrameIndex =  static_cast<uint64_t>(curr_val);
        if (curr_val != 0){
            /**Keeping max index of frame referenced in a table */
            if (new_FrameIndex > maxFrameI) maxFrameI = new_FrameIndex;

            if (isFrameEmpty(new_FrameIndex) && (avoid != new_FrameIndex)){ /*If avoid[curr_depth]*/
                /** New frame pointed from the table in frame index is empty, and we can now return
                 * This is the only place where curr_parentFrame needs to be updated*/
                curr_parentFrame = frameIndex;
                return new_FrameIndex;

            } else if ((new_FrameIndex=findEmptyTable(curr_depth+1, new_FrameIndex, frameIndex, maxFrameI, avoid)) != 0) {
                /**one of the tables in the level's below are empty, and they update the parent if needed*/
                return new_FrameIndex;
            }
        }
    }
}

uint64_t calc_cyclic_min(uint64_t pageNumber, uint64_t otherPageNumber){
    uint64_t absl=0;
    if (pageNumber > otherPageNumber) absl = pageNumber - otherPageNumber;
    else absl = otherPageNumber - pageNumber;

    if ((NUM_PAGES - absl) < absl) return (NUM_PAGES - absl);
    return absl;
}

void find_frameToEvict(uint64_t pageNumber, uint64_t& avoid[TABLES_DEPTH], uint64_t curr_page, uint64_t parent, uint64_t &maxparent,
                          uint64_t &max_page, uint64_t &max_cyclic_dist, uint64_t &max_phys_fr, uint64_t curr_depth){
    /**Recursively find the page (not in use by ours) with the maximal cyclic distance (kept by reference */
    if (curr_depth == TABLES_DEPTH) { /**Here it is already the (virtual) page */
        for (int j = 0; j < TABLES_DEPTH; ++j) {
            if (avoid[j] == curr_page) return;
        }
        if (calc_cyclic_min(pageNumber, curr_page) > max_cyclic_dist){
            max_page = curr_page;
            max_cyclic_dist = calc_cyclic_min(pageNumber, curr_page);
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
    /**For recursive call, and initializes evicted fram to zero and removes the reference to it */
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
