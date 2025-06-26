/**************************************************************
* Class::  CSC-415-02 Fall 2024
* Name:: Jonathan Curimao, Jonathan Ho, Yashwardhan Rathore, Juan Segura Rico
* Student IDs:: 921855139, 923536806, 921725126, 921759459
* GitHub-Name:: jonathancurimao
* Group-Name:: Team X
* Project:: Basic File System
*
* File:: parsePath.c
*
* Description:: This file contains the Parse Path functions.
*
**************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parsePath.h"
#include "dirEntry.h"
#include "volumeControlBlock.h"

extern VCB* vcb;
extern dirEntry* rootDir;
extern dirEntry* currWorkDir;
extern int pathMaxLength;


char* pathCleaner(char* path) {
if (path == NULL) {
        printf("Error cleaning string from pathCleaner(1)\n");
        return NULL;
    }

    // Duplicate path for safety
    char *pathCpy = strdup(path);
    if (pathCpy == NULL) {
        printf("Error cleaning string from pathCleaner(2)\n");
        return NULL;
    }
    
    char* saveptr = NULL;
    char* currToken = strtok_r(pathCpy, "/", &saveptr);

    // Initialize array to hold which tokens we want for final assembly
    int maxSize = 100;
    int pathArray[maxSize];

    // Initialize pathArray to -1 for stop condition
    for (int j = 0; j < maxSize; j++) {
        pathArray[j] = -1;
    }

    char* tokArray[maxSize];

    int pathIndex = 0;
    int i = 0;

    // Tokenize string and push them back onto our tokArray
    while (currToken != NULL) {
        tokArray[i] = currToken;

        // If valid token, save location of token into our pathArray array
        if (strcmp(currToken, ".") != 0 && strcmp(currToken, "..") != 0) {
            pathArray[pathIndex] = i;
            pathIndex++;
        }

        else if (strcmp(currToken, "..") == 0) {    // "..", Go back a directory
            if (pathIndex > 0) {
                pathIndex--;
                pathArray[pathIndex] = -1;
            }
        }

        i++;
        currToken = strtok_r(NULL, "/", &saveptr);
    }

    // Temporary holder path to assemble tokArray elements
    char* tempPath = malloc(pathMaxLength);
    tempPath[0] = '\0';     // Initialize first char to null, or else we get garbage chars
    tempPath[pathMaxLength] = '\0';
    for (int j = 0; pathArray[j] != -1; j++) {

        strcat(tempPath, tokArray[pathArray[j]]);

        // concatenate "/" between folders
        if (strcmp(tokArray[pathArray[j]], "/") != 0) {
            strcat(tempPath, "/");
        }
    }


    // Assemble final path; add "/" to beginning of path if root dir
    char* finalPath = malloc(pathMaxLength);
    finalPath[0] = '\0';
    finalPath[pathMaxLength] = '\0';
    if (pathCpy[0] == '/') {
        finalPath[0] = '/';
    }
    strcat(finalPath, tempPath);

    free(pathCpy);
    free(tempPath);
    tempPath = NULL;
    pathCpy = NULL;

    return finalPath;
}

// If valid path, returns 0. Also returns parent directory, name, and index of desired directory.
// Index is -1 if last directory does not exist.
// If invalid path, returns -1.
// If root directory, or "/", returns -2. 
int parsePath(char* path, pathInfo* ppi) {
    int checkpoint = 1;     // Debug number - delete later
    char debug[] = "parsePath checkpoint:";

    // printf("\t%s %d\n", debug, checkpoint);
    // checkpoint++;


    if (rootDir == NULL) {
        loadRoot();
    }

    char delimiter = '/';
    int lengthPath = strlen(path);
    dirEntry* start = NULL;
    dirEntry* parent = NULL;
    int index = -1;

    // Empty path entered
    if (lengthPath == 0) {
        return -1;
    }

    if (path[0] == '/') {
        // Set start path equal to root directory
        start = rootDir;
    }

    else {
        start = currWorkDir;
    }

    parent = start;
    char* saveptr = NULL;
    char* currToken = strtok_r(path, "/", &saveptr);
    char* nextToken = NULL;

    // Implies root directory ONLY
    if (currToken == NULL) {
        ppi->parentIndex = 0;
        ppi->lastElementName = NULL;
        ppi->returnParent = parent;
        return -2;
    }

    // cwd: /
    // foo/some2
    while (currToken != NULL) {
        nextToken = strtok_r(NULL, "/", &saveptr);

        // Determine if currToken dir exists in parent or not
        index = findEntryInDir(parent, currToken);

        // Reached end of string. Returns 0 regardless of if index is valid or not.
        // If index is -1, then end directory does not exist.
        if (nextToken == NULL) {
            ppi->parentIndex = index;
            ppi->returnParent = parent;
            ppi->lastElementName = currToken;

            return 0;
        }

        // Path does not exist
        if (index == -1) {
            return -1;
        }

        // Desired index is not a directory
        if (parent[index].isDir != 1) {
            return -1;
        }

        // Desired directory is found within parent dir; load it into RAM 
        parent = loadDir(&parent[index]);
        if (parent == NULL) {
            printf("Error: failed to load parent directory from parsePath.c\n");
            ppi->parentIndex = -1;
            // ppi->lastElementName = NULL;
            ppi->returnParent = parent;
            return -1;
        }

        currToken = nextToken;
    }
}

// Loads a directory from its address into memory. Returns a pointer to that directory.
// On error, returns NULL.
dirEntry* loadDir(dirEntry* dir) {

    if (dir == NULL) {
        return NULL;
    }

    // printf("loadDir name: %s\n", dir->fileName);

    if (dir->isDir == false) {
        return NULL;
    }

    int memBlocksNeeded = (dir->fileSize + (vcb->blockSize - 1)) / vcb->blockSize;
    int memBytesNeeded = memBlocksNeeded * vcb->blockSize;

    dirEntry* newDir = malloc(memBytesNeeded);
    newDir[0].fileLocation = dir[0].fileLocation;

    LBAread(newDir, memBlocksNeeded, newDir->fileLocation);

    // printf("loadDir name: %s\n", newDir[0].fileName);

    return newDir;
}


// Load root directory into rootDir pointer. Check if rootDir is already loaded into RAM before using this!
// Can check with "if (rootDir == NULL)"
// On success, returns 0. On failure, returns -1.
int loadRoot() {

    if (rootDir != NULL) {
        printf("Root already loaded. Returning...\n");
        return 0;
    }

    // Read start of our root directory into a temporary buffer
    // Need to get size of root directory so we can malloc it
    dirEntry* temp = malloc(vcb->blockSize);
    
    if (temp == NULL) {
        printf("Error in loadRoot!(1)\n");
        return -1;
    }
    LBAread(temp, 1, vcb->rootDirStart);

    // Get size of root directory
    int memBlocksNeeded = (temp->fileSize + (vcb->blockSize - 1)) / vcb->blockSize;
    int memBytesNeeded = memBlocksNeeded * vcb->blockSize;
    free(temp);


    rootDir = malloc(memBytesNeeded);
    if (rootDir == NULL) {
        printf("Error in loadRoot!(2)\n");
        return -1;
    }
    if (LBAread(rootDir, memBlocksNeeded, vcb->rootDirStart) != memBlocksNeeded) {
        printf("LBAread in loadRoot failed!\n");
        return -1;
    }

    return 0;
}

// Loops through a directory and finds corresponding directory entry.
// On success, returns index of corresponding dirEntry. If not found, returns -1.
int findEntryInDir(dirEntry* currDirectory, char* name) {
    if (currDirectory == NULL || name == NULL) {
        return -1;
    }

    // Get number of entries
    int numEntries = currDirectory[0].fileSize / sizeof(dirEntry);

    // Iterate through directory and find matching name
    for (int i = 0; i < numEntries; i++) {

        if (currDirectory[i].isUsed == true) {
            if (strcmp(name, currDirectory[i].fileName) == 0) {
                return i;
            }
        }
        
    }

    return -1;
}

// Gets the current working directory as a string.
char* getCWDStr() {
    return currWorkDir->fileName;

}

// Prefer using this method over using manual free() calls if possible!
// Frees allocated memory for pathInfo structure.
void freepathInfo(pathInfo* ppi) {
    if (ppi != NULL) {
        if (ppi->returnParent != NULL) {
            if (ppi->returnParent != rootDir && ppi->returnParent != currWorkDir) {
                free(ppi->returnParent);
            }
        }

        free(ppi);
    }

    ppi = NULL;
}

// Frees the current working directory string and resets the stack.
void freeSTRCWD() {

if (currWorkDir) {
        free(currWorkDir);
    }

}