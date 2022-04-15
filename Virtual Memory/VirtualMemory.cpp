#include "VirtualMemory.h"
#include "PhysicalMemory.h"

#define SUCCESS 1
#define FAILURE 0

using namespace std;

void clearTable(uint64_t frameIndex) {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

/*
 * Initialize the virtual memory
 */
void VMinitialize() {
    clearTable(0);
}

/*
 * Calculates if current frame/page has odd or even weight
 */
int oddOrEven(uint64_t offset) {
    return (offset & (uint64_t) 1) == 1 ? WEIGHT_ODD : WEIGHT_EVEN;
}

/*
 * finds an empty frame (with no kids).
 */
void findEmptyFrame(uint64_t frame, uint64_t parent, uint64_t frameOffset, int curDepth, uint64_t* unused, uint64_t doNotEvict) {
    if(curDepth == TABLES_DEPTH or frame == doNotEvict)
        return;
    word_t temp = 0;
    bool flag = true;
    for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset) {
        PMread((frame * PAGE_SIZE) + offset, &temp);
        if (temp != 0) {
            flag = false;
            findEmptyFrame(temp, frame, offset, curDepth + 1, unused, doNotEvict);
        }
    }
    if (flag) {
        PMwrite((parent * PAGE_SIZE) + frameOffset, 0);
        *unused = frame;
    }
}

/*
 * finds an unused frame. both case 2 and 3 on the description.
 */
void findUnused(uint64_t frame, int* maxFrameIndex, uint64_t curPageIndex, int curSum, int* maxSum,
                uint64_t* evictedPageIndex, uint64_t* evictedFrameIndex, uint64_t* evictedOffset, uint64_t* evictedParent, int curDepth) {
    if(curDepth == TABLES_DEPTH)
        return;
    word_t temp = 0;
    for (uint64_t offset = 0; offset < PAGE_SIZE; ++offset) {
        PMread((frame * PAGE_SIZE) + offset, &temp);
        if (temp != 0) {
            if (temp > *maxFrameIndex)
                *maxFrameIndex = temp;
            int tempSum = curSum + oddOrEven(temp);
            uint64_t tempPageIndex = (curPageIndex << OFFSET_WIDTH) + offset;
            if (curDepth == TABLES_DEPTH - 1)
                tempSum += oddOrEven(tempPageIndex);
            if (tempSum > *maxSum or (tempSum == *maxSum and tempPageIndex < *evictedPageIndex)) {
                *maxSum = tempSum;
                *evictedPageIndex = tempPageIndex;
                *evictedFrameIndex = temp;
                *evictedOffset = offset;
                *evictedParent = frame;
            }
            findUnused(temp, maxFrameIndex, tempPageIndex, tempSum,
                       maxSum, evictedPageIndex, evictedFrameIndex, evictedOffset, evictedParent, curDepth + 1);
        }
    }
}

/*
 * splits the given virtual address into parts and offset.
 */
void splitVirtualAddress(uint64_t address, uint64_t* addresses, uint64_t* offset){
    *offset = address & uint64_t((1 << OFFSET_WIDTH) - 1);
    int curIndex = TABLES_DEPTH - 1;
    uint64_t reminder = address >> OFFSET_WIDTH;
    uint64_t logPageSize = OFFSET_WIDTH;
    while (curIndex >= 0){
        uint64_t pageAddress = reminder & (PAGE_SIZE - 1);
        addresses[curIndex] = pageAddress;
        reminder >>= logPageSize;
        curIndex--;
    }
}

/*
 * Mutual function for Read and Write. finds the frame to read/write from/into.
 */
uint64_t VMFindAddress(uint64_t virtualAddress, uint64_t* offset) {
    uint64_t splitAddr[TABLES_DEPTH];
    int unusedFrame = 0;
    uint64_t evictedPageIndex = 0;
    uint64_t evictedFrameIndex = 0;
    uint64_t evictedOffset = 0;
    uint64_t evictedParent = 0;
    splitVirtualAddress(virtualAddress, splitAddr, offset);
    word_t addr[TABLES_DEPTH + 1];
    addr[0] = 0;
    for (int i = 1; i < TABLES_DEPTH + 1; ++i) {
        uint64_t emptyFrame = 0;
        int maxSum = -1;
        PMread((addr[i - 1] * PAGE_SIZE) + splitAddr[i - 1], &addr[i]);
        if (addr[i] == 0) {
            findUnused(0, &unusedFrame, 0, 0, &maxSum, &evictedPageIndex, &evictedFrameIndex, &evictedOffset, &evictedParent, 0);
            if (++unusedFrame >= NUM_FRAMES) {
                findEmptyFrame(0, 0, 0, 0, &emptyFrame, addr[i - 1]);
                if (emptyFrame == 0) {
                    PMevict(evictedFrameIndex, evictedPageIndex);
                    unusedFrame = evictedFrameIndex;
                    clearTable(unusedFrame);
                    PMwrite((evictedParent * PAGE_SIZE) + evictedOffset, 0);
                }
                else
                    unusedFrame = emptyFrame;
            }
            clearTable(unusedFrame);
            if (i == TABLES_DEPTH)
                PMrestore(unusedFrame, virtualAddress >> OFFSET_WIDTH);
            PMwrite((addr[i - 1] * PAGE_SIZE) + splitAddr[i - 1], unusedFrame);
            addr[i] = unusedFrame;
        }
    }
    return addr[TABLES_DEPTH];
}

/* reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
        return FAILURE;
    uint64_t offset;
    PMread((VMFindAddress(virtualAddress, &offset) * PAGE_SIZE) + offset, value);
    return SUCCESS;
}

/* writes a word to the given virtual address
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
        return FAILURE;
    uint64_t offset;
    PMwrite((VMFindAddress(virtualAddress, &offset) * PAGE_SIZE) + offset, value);
    return SUCCESS;
}
