/**************************************************************
* Class::  CSC-415-02 Fall 2024
* Name:: Jonathan Curimao, Jonathan Ho, Yashwardhan Rathore, Juan Segura Rico
* Student IDs:: 921855139, 923536806, 921725126, 921759459
* GitHub-Name:: jonathancurimao
* Group-Name:: Team X
* Project:: Basic File System
*
* File:: freeSpace.c
*
* Description:: File for implememntation of our free space management 
* for the file system.
*
**************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "freeSpace.h"
#include "volumeControlBlock.h"

#define BLOCK_SIZE 4096 // Example block size in bytes
#define NUM_OF_BLOCKS 512 // The total number of blocks


extern VCB* vcb;
unsigned char *bitmap = NULL; // Bitmap array for free blocks

// Initializes the free space bitmap
void initFreeSpace(int numOfBlocks) {

    (*vcb).numOfFreeBlocks = numOfBlocks;

    // Calculate number of bytes needed for the bitmap (1 bit per block)
    int bitmapSize = (numOfBlocks + 7) / 8; // round up to the nearest byte
    bitmap = (unsigned char *)malloc(bitmapSize);

    if (bitmap == NULL) {
        fprintf(stderr, "Failed to allocate memory for the bitmap\n");
        exit(1);
    }

    // Initialize all bits to 0 (all blocks are free initially)
    for (int i = 0; i < bitmapSize; i++) {
        bitmap[i] = 0;
    }
}

// Write bitmap to disk.
void writeBitmap() {
    if (bitmap == NULL) {
        printf("Error! No bitmap to write to disk!\n");
        return;
    }

    // Allocate space in memory for bitmap
    uint64_t blockSize = vcb->blockSize;
    int bitmapLocation = vcb->freeSpace;
    int bitmapSize = vcb->rootDirStart - vcb->freeSpace;

    int blocksWritten = LBAwrite(bitmap, bitmapSize, bitmapLocation);

    if (blocksWritten != bitmapSize) {
        printf("Error writing bitmap! Wrote %d blocks, Expected %d blocks written starting at block %d\n", blocksWritten, bitmapSize, bitmapLocation);
    }
}

// Load bitmap off disk.
void loadBitmap() {

    // Check if VCB is loaded into memory
    if (vcb == NULL ) {
        printf("Error loading bitmap! VCB not loaded!\n");
        return;
    }

    if (bitmap != NULL) {
        printf("Bitmap already loaded! Returning..\n");
        return;        
    }

    // Allocate space in memory for bitmap
    uint64_t blockSize = vcb->blockSize;
    int bitmapLocation = vcb->freeSpace;
    int bitmapSize = vcb->rootDirStart - vcb->freeSpace;

    bitmap = malloc(blockSize * bitmapSize);

    // printf("Attempting to load bitmap from disk...\n");

    int blocksRead = LBAread(bitmap, bitmapSize, bitmapLocation);

    if (blocksRead != bitmapSize) {
        printf("Error loading bitmap! Loaded %d blocks, Expected %d blocks read starting at block %d\n", blocksRead, bitmapSize, bitmapLocation);
    }
}

// Allocates closest free block, and returns the position of that block.
// If no blocks are free, returns -1.
// Legacy function - DO NOT USE! If possible, use allocateBlocks instead!
int allocateBlock() {
    if ((*vcb).numOfFreeBlocks == 0) {
        printf("No free blocks available\n");
        return -1;
    }

    for (int i = 0; i < (*vcb).numberOfBlocks; i++) {
        int byteIndex = i / 8;
        int bitIndex = i % 8;

        // Check if block is free (bit is 0)
        if (getBlockState(i) == 0) {
            // Mark the block as allocated (set bit to 1)
            bitmap[byteIndex] |= (1 << bitIndex);

            (*vcb).numOfFreeBlocks--;
            return i; // Return the allocated block number
        }
    }

    return -1; // No free block found
}

// Allocates a number of desiredBlocks, and returns position of starting block.
// If no blocks are free, returns -1.
int allocateBlocks(int desiredBlocks) {
    if (vcb->numOfFreeBlocks == 0) {
        printf("No free blocks available\n");
        return -1;
    }

    int startingBlockPos = 0;
    int numBlocksOpen = 0;

    for (int i = 0; i < vcb->numberOfBlocks; i++) {
        int byteIndex = i / 8;
        int bitIndex = i % 8;

        
        // Find nearest free block
        if (getBlockState(i) == 0) {
            startingBlockPos = i;
            
            // Loop through every single block after to check if we have desiredBlocks available
            for (int j = i; numBlocksOpen < desiredBlocks; j++) {

                if (getBlockState(j) == 0) {
                    numBlocksOpen++;
                }

                else {
                    numBlocksOpen = 0;
                    break;
                }
            }
        }

        if (numBlocksOpen == desiredBlocks) {
            break;
        }

    }

    // printf("Free block found at block %u\n", startingBlockPos);
    printf("Allocating %d blocks at %d\n", numBlocksOpen, startingBlockPos);

    if (numBlocksOpen >= desiredBlocks) {
        for (int i = startingBlockPos; desiredBlocks > 0; i++) {
            int byteIndex = i / 8;
            int bitIndex = i % 8;

            // Mark the block as allocated (set bit to 1)
            bitmap[byteIndex] |= (1 << bitIndex);

            vcb->numOfFreeBlocks--;
            desiredBlocks--;
        }

        return startingBlockPos;
    }

    return -1; // No free block found
}

// This will free a block located at index blockNumber (index starts from 0).
void freeBlock(int blockNumber) {
    if (blockNumber < 0 || blockNumber >= (*vcb).numberOfBlocks) {
        printf("Invalid block number\n");
        return;
    }

    int byteIndex = blockNumber / 8;
    int bitIndex = blockNumber % 8;

    // Check if block is allocated (bit is 1)
    if ((bitmap[byteIndex] & (1 << bitIndex)) != 0) {
        // Mark the block as free (set bit to 0)
        bitmap[byteIndex] &= ~(1 << bitIndex);

        (*vcb).numOfFreeBlocks++;
    }
}

// Print the bitmap for sample executions
void printBitmap() {
    for (int i = 0; i < (*vcb).numberOfBlocks; i++) {

        if (getBlockState(i) == 1) {
            printf("1");
        }

        else {
            printf("0");
        }

        if ((i + 1) % 8 == 0) {
            printf(" ");
        }
    }
    printf("\n");
}

// Clean up allocated memory from bitmap
void cleanupFreeSpace() {
    free(bitmap);
}

void printFreeSpace() {
    printf("Total blocks: %d\n", (*vcb).numberOfBlocks);
    printf("Free blocks: %d\n", (*vcb).numOfFreeBlocks);
    printf("Used blocks: %d\n", (*vcb).numberOfBlocks - (*vcb).numOfFreeBlocks);
}

// Check if a block at a given index is allocated or not. If allocated, returns 1. If not, returns 0.
// Index starts from 0 and ends at (superBlockObj.numOfBlocks - 1).
// If out of bounds or block is already taken, returns -1.
int getBlockState(int position) {

    // Error check that position is an inbounds bit.
    if (position > (*vcb).numberOfBlocks - 1) {
        printf("Error: bit %d is not valid!\n", position);
        return -1;
    }

    int byteIndex = position / 8;
    int bitIndex = position % 8;

    if ((bitmap[byteIndex] & (1 << bitIndex)) != 0) {
        return (1);
    }
    else {
        return (0);
    }
}

// int main() {
//     int numOfBlocks = 32; // Example number of blocks
//     initFreeSpace(numOfBlocks);

//     printf("Initial bitmap: ");
//     printBitmap();
//     printFreeSpace();

//     int allocatedBlock;
//     // Change testCases to test however many allocations you wish
//     int testCases = 4;
//     for (int i = 0; i < testCases; i++) {
//         printf("\n\nAllocation testing %d:\n", i);
//         allocatedBlock = allocateBlock();
//         if (allocatedBlock != -1) {
//         printf("Allocated block: %d\n", allocatedBlock);
//         printFreeSpace();
//         } else {
//             printf("No free blocks available\n");
//         }
//         printf("Bitmap after allocation: ");
//         printBitmap();
//     }

//     printf("\n\nFreeing block test:\n");
//     freeBlock(allocatedBlock);
//     printf("Bitmap after freeing block %d: ", allocatedBlock);
//     printBitmap();

//     cleanupFreeSpace();

    

//     return 0;
// }
