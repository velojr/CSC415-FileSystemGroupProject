/**************************************************************
* Class::  CSC-415-02 Fall 2024
* Name:: Jonathan Curimao, Jonathan Ho, Yashwardhan Rathore, Juan Segura Rico
* Student IDs:: 921855139, 923536806, 921725126, 921759459
* GitHub-Name:: jonathancurimao
* Group-Name:: Team X
* Project:: Basic File System
*
* File:: freeSpace.h
*
* Description:: This file contains the free space structure
*
**************************************************************/

#ifndef FREESPACE_H
#define FREESPACE_H

void initFreeSpace(int numOfBlocks);
void loadBitmap();
void writeBitmap();
int allocateBlock();
int allocateBlocks(int desiredBlocks);
void freeBlock(int blockNumber);
void printBitmap();
void cleanupFreeSpace();
void printFreeSpace();
int getBlockState(int position);

#endif