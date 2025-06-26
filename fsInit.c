/**************************************************************
* Class::  CSC-415-02 Fall 2024
* Name:: Jonathan Curimao, Jonathan Ho, Yashwardhan Rathore, Juan Segura Rico
* Student IDs:: 921855139, 923536806, 921725126, 921759459
* GitHub-Name:: jonathancurimao
* Group-Name:: Team X
* Project:: Basic File System
*
* File:: fsInit.c
*
* Description:: Main driver for file system assignment.
*
* This file is where you will start and initialize your system
*
**************************************************************/


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include "fsLow.h"
#include "mfs.h"
#include "dirEntry.h"
#include "freeSpace.h"
#include "volumeControlBlock.h"

#define MAGIC_NUMBER 0xDEADBEEF  // Replace with an appropriate unique number

extern VCB* vcb;
extern unsigned char *bitmap; // Bitmap array for free blocks
extern dirEntry* rootDir;


int initFileSystem (uint64_t numberOfBlocks, uint64_t blockSize) {
	printf ("Initializing File System with %ld blocks with a block size of %ld\n", numberOfBlocks, blockSize);

	// Step 1: Allocate memory for the VCB and read it off disk
	loadVCB(blockSize);

    // Error check that initialization was successful
    if (vcb == NULL) {
        printf("Error malloc'ing VCB!\n");
        return -1;
    }


    // Check if our disk is already formatted
    if (vcb->magicNumber == MAGIC_NUMBER) {
        printf("Volume already initialized. No formatting needed.\n");
    } 

    else {
        printf("Volume not initialized. Setting up new VCB.\n");

        // INITIALIZE VCB
        vcb->magicNumber = MAGIC_NUMBER;
        vcb->blockSize = blockSize;
        vcb->numberOfBlocks = numberOfBlocks;
        vcb->freeSpace = 0;         // This will be updated during free space initialization
        vcb->rootDirStart = 0;      // This will be updated during root directory initialization

        
        // INITIALIZE FREE SPACE MAP
        printf("Initializing free space map..\n");
        initFreeSpace(numberOfBlocks);


        // Reserve block 0 for vcb
        int blockLocation = allocateBlocks(1);
        if (blockLocation != 0) {
            printf("Error! Failed to initialize VCB!\n");
            return -1;
        }
        printf("\t...VCB initialized at block %d\n", blockLocation);


        // Declare where our free space map starts.
        vcb->freeSpace = blockLocation + 1;

        // Allocate appropriate number of blocks for freeSpace map.
        int numBytesFreeSpace = (numberOfBlocks + 7) / 8;
        int numBlocksFreeSpace = (numBytesFreeSpace + blockSize - 1) / blockSize;

        blockLocation = allocateBlocks(numBlocksFreeSpace);
        printf("\t...Free space map initialized at block %d\n", blockLocation);

        
        // INITIALIZE ROOT DIRECTORY and write to disk.
        dirEntry* rootDir = createDir(50, NULL, blockSize);
        if (rootDir == NULL) {
            printf("Error! Failed to create root directory!\n");
            return -1;
        }

        // Set vcb.rootDirStart to starting block of root directory.
        vcb->rootDirStart = rootDir[0].fileLocation;

        // Write freeSpace map to disk.
        writeBitmap();

        // Write VCB block to disk.
        LBAwrite(vcb, 1, 0);

        printf("New VCB initialized and written to disk.\n");
    }

    // Boot routine - load vcb, root, and set cwd to root
    loadVCB(blockSize);
    loadBitmap();
    if (loadRoot() == -1) {
        printf("Failed to load root.\n");
        return -1;
    }
    fs_setcwd("/");


    // TESTING FS FUNCTIONS
    // printf("\tTest FS functions...\n");
    // fs_mkdir("foo", 0);
    // fs_opendir("foo");
    // fdDir* currDir = fs_opendir("/");
    // while (fs_readdir(currDir) != NULL) {
    //     continue;
    // }

    return 0;
	}


void exitFileSystem ()
	{
	printf ("System exiting\n");
	}