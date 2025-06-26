/**************************************************************
* Class::  CSC-415-02 Spring 2024
* Name:: Jonathan Curimao, Jonathan Ho, Yashwardhan Rathore, Juan Segura Rico
* Student IDs:: 921855139, 923536806, 921725126, 921759459
* GitHub-Name:: jonathancurimao
* Group-Name:: Team X
* Project:: Basic File System
*
* File:: volumeControlBlock.h
*
* Description:: This file contains the VCB structure.
*
**************************************************************/
#ifndef VCB_H
#define VCB_H

#include <stdlib.h>

#include "fsLow.h"  // This file includes some useful typedefs

typedef struct {
    int magicNumber;     // Unique identifier to check if volume is initialized
    int blockSize;       // Size of each block
    int numberOfBlocks;  // Total number of blocks
    int numOfFreeBlocks; // Total number of free blocks
    int freeSpace;  // Start of free space map
    int rootDirStart;    // Start block of the root directory
} VCB;


void initVCB(uint64_t blockSize);
void loadVCB(uint64_t blockSize);
void freeVCB();

#endif