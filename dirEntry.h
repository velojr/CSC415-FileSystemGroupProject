/**************************************************************
* Class::  CSC-415-02 Fall 2024
* Name:: Jonathan Curimao, Jonathan Ho, Yashwardhan Rathore, Juan Segura Rico
* Student IDs:: 921855139, 923536806, 921725126, 921759459
* GitHub-Name:: jonathancurimao
* Group-Name:: Team X
* Project:: Basic File System
*
* File:: dirEntry.h
*
* Description:: This file contains the Directory Entry structure and related functions.
*
**************************************************************/
#ifndef DIRENTRY_H
#define DIRENTRY_H

#include <stdbool.h>
#include <time.h>

#include "fsLow.h"  // This file includes some useful typedefs
#define DIRECTORY 0x80000000  
typedef struct {
    char fileName[101];
    int fileLocation;   // Block location of file
    unsigned int fileSize;
    time_t dateCreated;
    time_t dateModified;
    unsigned int fileNumber;
    bool isDir;
    bool isUsed;
    int permissions;
} dirEntry;

// Set parent to NULL when creating the root!
dirEntry *createDir(int numEntries, dirEntry *parent, uint64_t blockSize);
int findUnusedDE(dirEntry *directory);
int deleteDE(char* pathname);

#endif