/**************************************************************
* Class::  CSC-415-02 Fall 2024
* Name:: Jonathan Curimao, Jonathan Ho, Yashwardhan Rathore, Juan Segura Rico
* Student IDs:: 921855139, 923536806, 921725126, 921759459
* GitHub-Name:: jonathancurimao
* Group-Name:: Team X
* Project:: Basic File System
*
* File:: dirEntry.c
*
* Description:: This file defines the functions relating to directory entries.
*
**************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "dirEntry.h"
#include "freeSpace.h"
#include "parsePath.h"
#include "volumeControlBlock.h"

extern VCB* vcb;
extern unsigned char *bitmap; // Bitmap array for free blocks

// Creates a directory entry. On success, returns a pointer to the new directory entry.
// Returns NULL on error.
dirEntry *createDir(int numEntries, dirEntry *parent, uint64_t blockSize) {
    int size = sizeof(dirEntry);
    // printf("Size of dirEntry: %d\n", size);

    int bytesNeeded = numEntries * size;
    int blocksNeeded = (bytesNeeded + (blockSize - 1)) / blockSize;

    // We can fit in more directory entries to minimize byte wastage
    int actBytes = blockSize * blocksNeeded;
    int actEntries = actBytes / size;


    dirEntry* newDir = malloc(actBytes);
    if (newDir == NULL) {
        printf("dirEntry.c: Error malloc'ing bytes!");
        return NULL;
    }
    
    // Allocate blocks
    int blockPosition = allocateBlocks(blocksNeeded);
    if (blockPosition == -1) {
        printf("dirEntry.c: Error allocating blocks\n");
        return NULL;
    }

    // printf("dirEntry.c: Attempting to initialize entries..\n");

    // Initialize empty directory elements
    time_t current = time(NULL);

    for (int i = 2; i < actEntries; i++) {
        strcpy(newDir[i].fileName, "\0");
        newDir[i].fileLocation = -1;            // Negative file location to indicate uninitialized
        newDir[i].fileSize = 0;
        newDir[i].dateCreated = current * -1;   // Negative time to indicate uninitialized
        newDir[i].dateModified = current * -1;
        newDir[i].fileNumber = -1;              // Negative file number to indicate uninitialized
        newDir[i].isDir = false;
        newDir[i].isUsed = false;

        // TODO: Set file permissions
    }

    // Initialize dirEntry[0] and dirEntry[1]
    strcpy(newDir[0].fileName, ".");
    newDir[0].fileLocation = blockPosition;
    newDir[0].fileSize = actEntries * sizeof(dirEntry);
    newDir[0].dateCreated = current;
    newDir[0].dateModified = current;
    newDir[0].fileNumber = 0;
    newDir[0].isDir = true;
    newDir[0].isUsed = true;

    // If current folder IS the root folder
    if (parent == NULL) {
        parent = newDir;
    }
    strcpy(newDir[1].fileName, "..");
    newDir[1].fileLocation = parent[0].fileLocation;
    newDir[1].fileSize = parent[0].fileSize;
    newDir[1].dateCreated = parent[0].dateCreated;
    newDir[1].dateModified = parent[0].dateModified;
    newDir[1].fileNumber = parent[0].fileNumber;
    newDir[1].isDir = parent[0].isDir;
    newDir[1].isUsed = parent[0].isDir;
    
    // printf("New directory created at block %d.\n", newDir[0].fileLocation);



    // printf("Attempting to write directory..\n");

    // Check for expected number of blocks written
    int retVal = LBAwrite(newDir, blocksNeeded, blockPosition);
    if (retVal != blocksNeeded) {
        printf("Error writing directory! Expected %d blocks, got %d instead.\n", blocksNeeded, retVal);
        return NULL;
    }
    // printf("\tSuccessfully written!\n");

    // Update bitmap.
    writeBitmap();

    return newDir;
}

// Finds the first unused directory entry in the specified directory.
// Returns the index of the unused entry, or -1 if no free entries are found.
int findUnusedDE(dirEntry *directory) {
    int numEntries = directory[0].fileSize / sizeof(dirEntry);

    for (int i = 2; i < numEntries; i++) { // Start at index 2 to skip "." and ".."
        // printf("directory %d isUsed: %d\n", i, directory[i].isUsed);
        if (directory[i].isUsed == false) {
            return i;  // Found an unused entry
        }
    }
    return -1;  // No unused entries found
}

int deleteDE(char* pathname) {

    pathInfo *ppi = malloc(sizeof(pathInfo));
    ppi->lastElementName = NULL;
    ppi->returnParent = NULL;
    ppi->parentIndex = -1;

    char* pathCpy = strdup(pathname);
    int retVal = parsePath(pathCpy, ppi);

    free(pathCpy);
    pathCpy = NULL;

    if (retVal == -1) {
        printf("Error parsing path in deleteDE\n");
        return -1;
    }

    else if (retVal == -2) {
        printf("Error! Trying to remove root directory!\n");
        return -1;
    }

    int blockSize = vcb->blockSize;
    dirEntry *parentDir = ppi->returnParent;        // Get parent of desired dir
    int fileLocation = ppi->returnParent[ppi->parentIndex].fileLocation;    // Get location of directory
    int numBlocksUsed = (ppi->returnParent[ppi->parentIndex].fileSize + blockSize - 1) / blockSize;   // Get how many blocks are used by dir
    
    parentDir[ppi->parentIndex].isUsed = false;

    // Free blocks in VCB.
    for (int numBlocksFreed = 0; numBlocksFreed < numBlocksUsed; numBlocksFreed++) {
        freeBlock(fileLocation);
        fileLocation++;
    }

    // Save modified bitmap back to disk.
    writeBitmap();

    // Write modified parent directory back to disk.
    int parentBlocks = (parentDir[0].fileSize + blockSize - 1) / blockSize;
    if (LBAwrite(parentDir, parentBlocks, parentDir[0].fileLocation) != parentBlocks) {
        printf("rmdir failed to write to disk\n");
        return -1;
    }
    
    freepathInfo(ppi);
    parentDir = NULL;
    ppi = NULL;
}