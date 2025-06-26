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
* Description:: This file contains the VCB functions.
*
**************************************************************/

#include <stdio.h>

#include "volumeControlBlock.h"

VCB* vcb = NULL;

void initVCB(uint64_t blockSize) {
    vcb = (VCB *)malloc(blockSize);
}

// Load VCB into ram.
// CALL THIS EVERY TIME ON A FRESH BOOT FOR SAFETY!!!
void loadVCB(uint64_t blockSize) {
    // Check if VCB is already loaded.
    if (vcb != NULL) {
        printf("Error! VCB already loaded!\n");
        return;
    }
    
    initVCB(blockSize);
    if (LBAread(vcb, 1, 0) != 1) {
        printf("Error! Failed to load VCB into memory!\n");
    }
}

void freeVCB() {
    free(vcb);
    vcb = NULL;
}