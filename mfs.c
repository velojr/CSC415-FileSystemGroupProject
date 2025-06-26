/**************************************************************
* Class::  CSC-415-02 Fall 2024
* Name:: Jonathan Curimao, Jonathan Ho, Yashwardhan Rathore, Juan Segura Rico
* Student IDs:: 921855139, 923536806, 921725126, 921759459
* GitHub-Name:: jonathancurimao
* Group-Name:: Team X
* Project:: Basic File System
*
* File:: mfs.c
*
* Description:: File to implement core functions for our system.
*
**************************************************************/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "mfs.h"
#include "freeSpace.h"
#include "volumeControlBlock.h"

extern VCB* vcb;
dirEntry* rootDir = NULL;
dirEntry* currWorkDir = NULL;
char *currWorkPath = NULL;
int pathMaxLength = 2048;

// Key directory functions
int fs_mkdir(const char *pathname, mode_t mode) {
    if (pathname == NULL || vcb == NULL) {
        printf("Error: Invalid pathname or uninitialized VCB\n");
        return -1;
    }

    pathInfo *ppi = malloc(sizeof(pathInfo));
    if (ppi == NULL) {
        printf("Error allocating memory for pathInfo\n");
        return -1;
    }
    ppi->returnParent = NULL;
    ppi->lastElementName = NULL;
    ppi->parentIndex = 0;

    char *pathCpy = strdup(pathname);
    int retVal = parsePath(pathCpy, ppi);

    if (retVal == -1) {                 // Check for parse errors
        printf("Error parsing path: %s does not exist\n", pathCpy);
        freepathInfo(ppi);
        free(pathCpy);
        ppi = NULL;
        pathCpy = NULL;
        return -1;
    }

    else if (retVal == -2) {            // Trying to create already existing root
        printf("Error: Root directory already exists\n");
        freepathInfo(ppi);
        free(pathCpy);
        ppi = NULL;
        pathCpy = NULL;
        return -1;
    }

    else if (ppi->parentIndex != -1) {  // Check if directory already exists
        printf("Directory already exists: %s\n", ppi->lastElementName);
        freepathInfo(ppi);
        free(pathCpy);
        ppi = NULL;
        pathCpy = NULL;
        return -1;
    }

    int freeIndex = findUnusedDE(ppi->returnParent);  // Find available slot in parent
    if (freeIndex == -1) {
        printf("No available entries in parent directory\n");
        freepathInfo(ppi);
        free(pathCpy);
        ppi = NULL;
        pathCpy = NULL;
        return -1;
    }

    uint64_t blockSize = vcb->blockSize;  // Use VCB block size
    int numEntries = 50;  // Initial entries in the new directory
    dirEntry *newDir = createDir(numEntries, ppi->returnParent, blockSize);

    if (newDir == NULL) {
        printf("Error creating new directory\n");
        freepathInfo(ppi);
        free(pathCpy);
        ppi = NULL;
        pathCpy = NULL;
        return -1;
    }

    // Set directory entry properties
    newDir[0].permissions = mode | DIRECTORY;
    strncpy(ppi->returnParent[freeIndex].fileName, ppi->lastElementName, sizeof(ppi->returnParent[freeIndex].fileName) - 1);
    ppi->returnParent[freeIndex].fileLocation = newDir[0].fileLocation;
    ppi->returnParent[freeIndex].fileSize = newDir[0].fileSize;
    ppi->returnParent[freeIndex].isDir = true;
    ppi->returnParent[freeIndex].isUsed = true;


    int parentFileSize = (ppi->returnParent[0].fileSize + blockSize - 1) / blockSize;

    // Rewrite whole parent directory to disk in order to update entries
    LBAwrite(ppi->returnParent, parentFileSize, ppi->returnParent[0].fileLocation);

    printf("%s created successfully.\n", pathCpy);

    free(newDir);
    freepathInfo(ppi);
    free(pathCpy);
    newDir = NULL;
    pathCpy = NULL;

    return 0;
}

int fs_rmdir(const char *pathname) {

    // Here we will check to see if the directory exists.
    fdDir *dir = NULL; 
    if ((dir = fs_opendir(pathname)) == NULL) {
        printf("fs_rmdir: Error opening directory.\n");
        return -1;
    }

    // fs_readdir returns a pointer that is a child of dir, so we need to make a pointer to it
    struct fs_diriteminfo* currItem = dir->di;

    // Here, we will check to see if the directory is empty
    int isEmpty = 1;
    while (currItem != NULL) {
        if (strcmp(currItem->d_name, ".") != 0 && strcmp(currItem->d_name, "..") != 0) {
            isEmpty = 0;
            break;
        }

        currItem = fs_readdir(dir);
    }
    fs_closedir(dir);
    dir = NULL;

    if (!isEmpty) {
        fprintf(stderr, "Directory '%s' is not empty.\n", pathname);
        return EEXIST;
    }

    char* pathCpy = strdup(pathname);
    // Call delete directory entry routine
    if (deleteDE(pathCpy) == -1) {
        printf("fs_rmdir Failed to delete directory!\n");
        return -1;
    }
    free(pathCpy);
    pathCpy = NULL;

    printf("Directory '%s' removed successfully.\n", pathname);
    return 0;
}

// Directory iteration functions

// Open a directory and return a pointer to it.
fdDir * fs_opendir(const char *pathname) {

    // Create new pathInfo object
    pathInfo* parentInfo = malloc(sizeof(pathInfo));
    parentInfo->parentIndex = 0;
    parentInfo->lastElementName = NULL;
    parentInfo->returnParent = NULL;

    // Create new fdDir object
    // Malloc'd "fdDir* currDir"; free this in closedir!
    fdDir* currDir = malloc(sizeof(fdDir));
    currDir->d_reclen = sizeof(fdDir);
    currDir->dirEntryPosition = 0;
    currDir->directory = NULL;
    currDir->di = NULL;

    char *pathCpy = strdup(pathname);
    int retVal = parsePath(pathCpy, parentInfo);


    // Declare desiredDir to pass into currDir.directory later on
    dirEntry* desiredDir = NULL;

    // Empty file path. "D" is appended to an empty ls call.
    if (strcmp("D", pathCpy) == 0) {
        printf("No path entered; using currWorkDir\n");
        desiredDir = currWorkDir;
    }

    // Error check for valid path
    else if (retVal == -1 || parentInfo->parentIndex == -1) {
        printf("fs_opendir Error; %s is invalid\n", pathCpy);
        return NULL;
    }

    // Root directory
    else if (retVal == -2) {
        desiredDir = rootDir;
    }

    // If not root directory, search parent directory for directory with corresponding name 
    else {
        int index = parentInfo->parentIndex;

        desiredDir = loadDir(&parentInfo->returnParent[index]);
        if (desiredDir == NULL) {
            printf("Error finding directory!\n");
            return NULL;
        }
    }

    // Load desiredDir into RAM
    // BE CAREFUL!! Only free currDir->directory if it is not equal to root AND cwd!!
    currDir->directory = desiredDir;
    desiredDir = NULL;

    // printf("fileName: %s\n", currDir->directory[0].fileName);
    // printf("isUsed: %d\n", currDir->directory[0].isUsed);
    // printf("isDir: %d\n", currDir->directory[0].isDir);


    struct fs_diriteminfo *currItem = malloc(sizeof(struct fs_diriteminfo));
    strcpy(currItem->d_name, currDir->directory[0].fileName);
    // Since first item is "." folder, we can skip checking if first entry is folder or file
    currItem->fileType = MY_FS_DIR;
    currItem->d_reclen = sizeof(struct fs_diriteminfo);
    
    // "currDir->di" is malloc'd; free this in closedir!
    currDir->di = currItem;

    freepathInfo(parentInfo);
    free(pathCpy);
    parentInfo = NULL;
    pathCpy = NULL;
    
    return currDir;
}

struct fs_diriteminfo *fs_readdir(fdDir *dirp) {

    // Shorthand declaration for convenience
    struct fs_diriteminfo* currItem = dirp->di;
    int numEntries = dirp->directory[0].fileSize / sizeof(dirEntry);

    int i = dirp->dirEntryPosition;

    // Loop through directory til we find next valid entry
    for (i; i < numEntries ; i++) {
        if (dirp->directory[i].isUsed == true) {
            strcpy(currItem->d_name, dirp->directory[i].fileName);

            if (dirp->directory[i].isDir) {
                currItem->fileType = MY_FS_DIR;
            }

            else {
                currItem->fileType = MY_FS_FILE;
            }

            break;
        }
    }

    // We've run out of entries to look through
    if (i == numEntries) {
        return NULL;
    }

    dirp->dirEntryPosition++;
    return currItem;
}

int fs_closedir(fdDir *dirp) {
    // Shorthand declaration for convenience
    struct fs_diriteminfo* currItem = dirp->di;

    if (dirp == NULL) {
        printf("Error! No directory opened!");
        return -1;
    }

    // DO NOT FREE THIS DIRECTORY! It could be the root or cwd!
    if (dirp->directory != rootDir && dirp->directory != currWorkDir) {
        free(dirp->directory);
        dirp->directory = NULL;
    }

    free(dirp->di);
    free(dirp);
    dirp->di = NULL;
    dirp = NULL;

    return 0;
}

// Misc directory functions
char * fs_getcwd(char *pathname, size_t size) {
    strncpy(pathname, currWorkPath, size);
}

int fs_setcwd(char *pathname) {
    // Duplicate pathname to avoid modifying the original
    char* pathCopy = strdup(pathname);
    struct pathInfo* currInfo = malloc(sizeof(struct pathInfo));

    int retVal = parsePath(pathCopy, currInfo);

    if (retVal == -1 || currInfo->parentIndex < 0) {
        // printf("Invalid path, working directory unchanged\n");
        free(currInfo);
        free(pathCopy);
        currInfo = NULL;
        pathCopy = NULL;
        return -1;
    }

    dirEntry *desiredDir = NULL;

    // If not done so, allocate memory for currWorkPath
    if (currWorkPath == NULL) {
        currWorkPath = malloc(pathMaxLength);
        currWorkPath[0] = '\0';
        currWorkPath[pathMaxLength - 1] = '\0';
    }

    // Root directory
    if (retVal == -2) {
        desiredDir = rootDir;
        strcpy(currWorkPath, "/");
    }

    // If not root directory, load desiredDir into memory 
    else {
        int index = currInfo->parentIndex;

        desiredDir = loadDir(&currInfo->returnParent[index]);
        if (desiredDir == NULL) {
            printf("Error finding directory!\n");
            return -1;
        }

        // Change currWorkPath
        if (pathCopy[0] == '/') {
            strcpy(currWorkPath, pathname);
        }

        else {
            strcat(currWorkPath, pathname);
        }

        // This gives us a memory leak. We lost access to the original currWorkPath!
        currWorkPath = pathCleaner(currWorkPath);
    }

    // Load new working directory
    currWorkDir = desiredDir;

    // printf("Working directory changed to %s\n", currWorkPath);

    // printf("currworkpath: %s\n", currWorkPath);

    free(currInfo);
    free(pathCopy);
    currInfo = NULL;
    pathCopy = NULL;


    return 0;
}   //linux chdir

int fs_isFile(char * filename) { // This function will check if the given file path is a file or not.
    // struct fs_stat fileInfo; // The stat declaration is used to store the information about the file.

    // if (fs_stat(filename, &fileInfo) == -1) { // With fs_stat, information about the file can be retrieved.
    //     perror("Error retrieving file information.");
    //     return 0;
    // }


    // return MY_FS_FILE; // With MY_FS_FILE, it will check to see if it is a file.



    // Lets just call fs_isDir because we know it works!
    
    char* pathCpy = strdup(filename);

    int retVal = fs_isDir(pathCpy);

    if (retVal == -1) {
        printf("fs_isFile Error: invalid path\n");
        free(pathCpy);
        pathCpy = NULL;

        return -1;
    }

    else if (retVal == 1) {     // isDir returned a dir
        free(pathCpy);
        pathCpy = NULL;

        return 0;
    }

    free(pathCpy);
    pathCpy = NULL;

    return 1;   // isDir returned file

}	//return 1 if file, 0 otherwise

int fs_isDir(char * pathname) {

    char* pathCpy = strdup(pathname);

    // Initialize pathInfo obj
    pathInfo* ppi = malloc(sizeof(pathInfo));
    ppi->lastElementName = NULL;
    ppi->returnParent = NULL;

    int retVal = parsePath(pathCpy, ppi);
    if (retVal == -1 || ppi->parentIndex == -1) {
        printf("fs_isDir Error: invalid path\n");
        freepathInfo(ppi);
        free(pathCpy);
        pathCpy = NULL;

        return -1;
    }

    int index = ppi->parentIndex;
    if (ppi->returnParent[index].isDir == true) {
        freepathInfo(ppi);
        free(pathCpy);
        pathCpy = NULL;

        return 1;
    }

    freepathInfo(ppi);
    free(pathCpy);
    pathCpy = NULL;

    return 0;
} // return 1 if directory, 0 otherwise

int fs_delete(char* filename) {

    if (fs_isFile(filename) == 0) {
        printf("%s is not a file!\n", filename);
        return -1;
    }

    char* pathCpy = strdup(filename);
    // Call delete directory entry routine
    if (deleteDE(pathCpy) == -1) {
        printf("fs_delete Failed to delete file!\n");
        return -1;
    }
    free(pathCpy);
    pathCpy = NULL;

    printf("File '%s' removed successfully.\n", filename);
    return 0;

}	//removes a file

int fs_stat(const char *path, struct fs_stat *buf) {
    
    // Create a copy of the path to avoid modifying the original
    char *pathCpy = strdup(path);
    if (pathCpy == NULL) {
        printf("Error: Could not duplicate path\n");
        pathCpy = NULL;
        return -1;
    }
    // printf("path: %s\n", pathCpy);

    // Initialize pathInfo structure
    pathInfo *ppi = malloc(sizeof(pathInfo));
    ppi->lastElementName = NULL;
    ppi->returnParent = NULL;

    int retVal = parsePath(pathCpy, ppi);

    // Free the path copy
    free(pathCpy);
    pathCpy = NULL;

    if (retVal == -1) {
        printf("fs_stat: ERROR IN PARSE PATH: %d\n", retVal);
        return -1;
    }

    if (ppi->parentIndex == -1) {
        printf("fs_stat: Path does not exist.\n");
        return -1;
    }

    // Get the directory entry for the target
    int index = ppi->parentIndex;
    dirEntry *targetEntry = &ppi->returnParent[index];

    time_t dateAccessed = time(NULL);

    // Populate the fs_stat structure with directory entry information
    buf->st_size = targetEntry->fileSize;
    buf->st_blksize = vcb->blockSize; 
    buf->st_blocks = (targetEntry->fileSize + buf->st_blksize - 1) / buf->st_blksize;
    buf->st_accesstime = dateAccessed;
    buf->st_modtime = targetEntry->dateModified;
    buf->st_createtime = targetEntry->dateCreated;

    // No need to call free because this is stack-allocated
    freepathInfo(ppi);
    return 0;
}
