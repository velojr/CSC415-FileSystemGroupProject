/**************************************************************
* Class::  CSC-415-02 Fall 2024
* Name:: Jonathan Curimao, Jonathan Ho, Yashwardhan Rathore, Juan Segura Rico
* Student IDs:: 921855139, 923536806, 921725126, 921759459
* GitHub-Name:: jonathancurimao
* Group-Name:: Team X
* Project:: Basic File System
*
* File:: parsePath.h
*
* Description:: This file contains the Parse Path structure and related functions.
*
**************************************************************/
#ifndef PARSEPATH_H
#define PARSEPATH_H

#include "dirEntry.h"


typedef struct pathInfo {
 dirEntry* returnParent;    // Name of parent directory
 char* lastElementName;     // Name of desired item within parent directory
 int parentIndex;   // Index of desired item within parent directory

} pathInfo;

char* pathCleaner(char* path);
int parsePath(char* path, pathInfo* ppi);
dirEntry* loadDir(dirEntry* dir);
int loadRoot();
int findEntryInDir(dirEntry* currDirectory, char* name);
char* getCWDStr(); 
void freepathInfo(pathInfo* ppi);
void freeSTRCWD();

#endif