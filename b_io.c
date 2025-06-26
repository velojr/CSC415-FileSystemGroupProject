/**************************************************************
* Class::  CSC-415-02 Spring 2024
* Name:: Jonathan Curimao, Jonathan Ho, Yashwardhan Rathore, Juan Segura Rico
* Student IDs:: 921855139, 923536806, 921725126, 921759459
* GitHub-Name:: jonathancurimao
* Group-Name:: Team X
* Project:: Basic File System
*
* File:: b_io.c
*
* Description:: Basic File System - Key File I/O Operations
*
**************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>			// for malloc
#include <string.h>			// for memcpy
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "b_io.h"

#include "volumeControlBlock.h"     // for vcb
#include "freeSpace.h"              // for freeSpace functions
#include "parsePath.h"

#define MAXFCBS 20
#define B_CHUNK_SIZE 512

extern VCB* vcb;
extern unsigned char *bitmap;

typedef struct b_fcb
	{
	/** TODO add al the information you need in the file control block **/
	char * buf;		            // holds the open file buffer
	int index;		            // holds the current position in the buffer
	int buflen;		            // holds how many valid bytes are in the buffer
	int totalRead;              // holds the total amount of bytes read

	unsigned long position;     // holds the block location on disk
    unsigned int currBlock;     // indicates current block offset from block location
    unsigned int numBlocks;     // number of blocks allocated
	char * parentDir;
	} b_fcb;
	
b_fcb fcbArray[MAXFCBS];

int startup = 0;	//Indicates that this has not been initialized

//Method to initialize our file system
void b_init ()
	{
	//init fcbArray to all free
	for (int i = 0; i < MAXFCBS; i++)
		{
		fcbArray[i].buf = NULL; //indicates a free fcbArray
		}
		
	startup = 1;
	}

//Method to get a free FCB element
b_io_fd b_getFCB ()
	{
	for (int i = 0; i < MAXFCBS; i++)
		{
		if (fcbArray[i].buf == NULL)
			{
			return i;		//Not thread safe (But do not worry about it for this assignment)
			}
		}
	return (-1);  //all in use
	}
	
// Interface to open a buffered file
// Modification of interface for this assignment, flags match the Linux flags for open
// O_RDONLY, O_WRONLY, or O_RDWR
// TODO: Figure out where to call LBAwrite so that we can create the file.
// We need to call LBAwrite for two things: to update the directory and to actually create the file on disk.
b_io_fd b_open(char *filename, int flags) {
    b_io_fd returnFd;
    char *buffer;

    // Allocate memory for path parsing structure
    pathInfo *ppi = malloc(sizeof(pathInfo));
    if (!ppi) {
        printf("\tb_open: Error allocating memory for pathInfo.\n");
        return -1;
    }
    ppi->lastElementName = NULL;
    ppi->returnParent = NULL;

    // Duplicate the file path
    char *pathCpy = strdup(filename);
    if (!pathCpy) {
        printf("\tb_open: Error duplicating file path.\n");
        free(ppi);
        return -1;
    }

    // Parse the given file path and populate pathInfo structure
    if (parsePath(pathCpy, ppi) == -1) {
        printf("\tb_open: Error parsing path.\n");
        free(pathCpy);
        free(ppi);
        return -1;
    }

    // Check if the file exists in the parsed path
    dirEntry *currDir = &ppi->returnParent[0];
    if (ppi->parentIndex >= 0 && currDir[ppi->parentIndex].isDir) {
        printf("\tb_open: %s is not a file.\n", filename);
        free(pathCpy);
        free(ppi);
        return -1;
    }

    dirEntry *openFile = NULL;

    // Handle file creation if the 'create' flag is specified
    if (((flags >> 6) & 0x1) && ppi->parentIndex == -1) {
        printf("\tb_open: Creating file %s\n", ppi->lastElementName);

        // Allocate memory for a new directory entry
        openFile = malloc(sizeof(dirEntry));
        if (!openFile) {
            printf("\tb_open: Error allocating memory for dirEntry.\n");
            free(pathCpy);
            free(ppi);
            return -1;
        }

        // Copy the file name into the new directory entry
        int size = sizeof(openFile->fileName);
        strncpy(openFile->fileName, ppi->lastElementName, size);
        openFile->fileName[size - 1] = '\0';  // Ensures null termination
    } 
    // Handle the case where the file may not exist
    else if (((flags >> 6) & 0x0) && ppi->parentIndex == -1) {
        printf("\tb_open: File not found, create flag not specified.\n");
        free(pathCpy);
        free(ppi);
        return -1;
    } 
    // Handle opening an existing file
    else if (ppi->parentIndex >= 0) {
        printf("\tb_open: Opening file %s\n", filename);
    } 
    // Handle any error scenarios
    else {
        printf("\tb_open: Unexpected error opening file.\n");
        free(pathCpy);
        free(ppi);
        return -1;
    }

    // Initialize file system if not already initialized
    if (!startup) {
        b_init();
    }

    // Get an available FCB for the file
    returnFd = b_getFCB();
    if (returnFd < 0) {
        printf("\tb_open: No available FCB.\n");
        free(pathCpy);
        free(ppi);
        return -1;
    }

    // Allocate buffer memory for the file's FCB if not already allocated
    if (!fcbArray[returnFd].buf) {
        fcbArray[returnFd].buf = malloc(B_CHUNK_SIZE);
        if (!fcbArray[returnFd].buf) {
            printf("\tb_open: Error allocating file buffer.\n");
            free(pathCpy);
            free(ppi);
            return -1;
        }
    }

    // Initialize the FCB structure fields for the file
    fcbArray[returnFd].index = 0;
    fcbArray[returnFd].parentDir = NULL;
    fcbArray[returnFd].buflen = 0;
    fcbArray[returnFd].totalRead = 0;
    fcbArray[returnFd].position = 0;

    free(pathCpy);
    free(ppi);

    return returnFd; // Return the FCB index
}


// Interface to seek function	
int b_seek (b_io_fd fd, off_t offset, int whence)
	{
	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if (fd < 0 || fd >= MAXFCBS || fcbArray[fd].buf == NULL)
		{
		return (-1); 					//invalid file descriptor
		}

	b_fcb *fcb = &fcbArray[fd];

    int newPosition = fcb->index;  // Starting with the current position
		
	// Determine the new position based on 'whence'
    switch (whence) {
        case SEEK_SET:
            newPosition = offset;
            break;
        case SEEK_CUR:
            newPosition += offset;
            break;
        case SEEK_END:
            newPosition = fcb->buflen + offset; 
            break;
        default:
            return -1; // Invalid 'whence'
    }

	// Ensure that the new position is within bounds
    if (newPosition < 0 || newPosition > fcb->buflen) {
        return -1; // out of bounds
    }

	fcb->index = newPosition; // Update current position

    return fcb->index; // Returning the new position

	}



// Interface to write function	
// Take similar approach to b_read.
// Part 0: Calculate amount of space needed for each step
// Part 0.5: Allocate whatever space is predicted to be needed
// Part 0.55: If we allocate a new segment, we need to copy over the original data to the new segment!!
// Part 1: Write whatever space is reminaing of the file, from the buffer.
// Part 2: Allocate and write multiple blocks worth of data into file.
// Part 3: Write remainder of bytes into file.
// Part 4: Call LBAWrite and free original alloc'd blocks (if any).
int b_write (b_io_fd fd, char * buffer, int count)
	{
	if (startup == 0) b_init();  //Initialize our system

	// Validate the file descriptor
	if ((fd < 0) || (fd >= MAXFCBS) || (fcbArray[fd].buf == NULL))
		{
		return (-1); 					//invalid file descriptor
		}

	b_fcb *fcb = &fcbArray[fd];


    // Part 0: Calculate number of bytes for each step

    int part1 = 0;
    int part2 = 0;
    int part3 = 0;
    
    int bytesRemaining = fcb->buflen - fcb->index;
    if (bytesRemaining >= count) {  // We have enough bytes in buffer
        part1 = count;
    }

    else {
        part1 = bytesRemaining; // Commit bytesRemaining into buffer

        // Set part3 to remainder of count.
        part3 = count;
        part3 -= part1;

        // Set part2 to amount of bytes worth of full blocks we can commit
        int numBlocksToCopy = part3 / vcb->blockSize;
        part2 = numBlocksToCopy * vcb->blockSize;

        // Decrement part3 by how many bytes we commit in part2
        part3 -= part2;
    }

    int totalBytes = part1 + part2 + part3;         // We should get "count" number of bytes out of this
    if (totalBytes != count) {
        printf("\tb_write: Error, totalBytes calculation does not match count. Expected: %d, got: %d\n", count, totalBytes);
        return -1;
    }


    // Part 0.5: Check that we have enough blocks allocated
    // If not, allocate some number of initial blocks to the file

    // This will be tricky because we want to keep our file contiguous.
    // We may run into trouble if our file needs to allocate more blocks, but it becomes segmented
    // Ideally, we should allocate some number of blocks in order to grab a new contiguous segment, then FREE the original extent of blocks
    // To do this, we need to track the before/after of our file location and file size.
    // We also need to copy over the data from the original extent to the new extent.


    // TODO: Write a more complex algorithm for determining how many blocks to allocate
    // How do we do this? We can probably set up some kind of static fcb tracker variable
    // We then compare that fcb variable from call to call to see if we are still in the same file
    // If we're still in the same file, then continuously ramp up number of blocks allocated
    // Else, reset number of blocks allocated back to 50

    static int initialAllocation = 50;    // Initial number of blocks to allocate
    initialAllocation *= 2;               // On first call, initialAllocation will actually be 100 blocks

    // Variables to track before/after of allocation
    int origBlocksAllocated = fcb->numBlocks;       // Keep track of how many blocks the original file was
    int newBlocksAllocated = origBlocksAllocated;
    int origLocation = fcb->position;               // Original position of file
    int newLocation = origLocation;
    
    // Calculate number of predicted blocks. If predictedBlocks < numBlocks, allocate more.
    int newBlocksToWrite = (part2 + part3 + vcb->blockSize - 1) / vcb->blockSize; 
    int totalPredictedBlocks = fcb->numBlocks + newBlocksToWrite;

    // Flag to indicate if we have a reallocation for cleanup later in Part 4
    int reAlloced = 0;

    if (totalPredictedBlocks < fcb->numBlocks) {

        reAlloced = 1;

        // Allocate new number of blocks and save new position
        newBlocksAllocated += initialAllocation;
        newLocation = allocateBlocks(newBlocksAllocated);

        // Save new location and new number of blocks allocated
        fcb->position = newLocation;
        fcb->numBlocks = newBlocksAllocated;

        // TODO: NEED TO COPY OVER ORIGINAL DATA TO NEW LOCATION!!

        // Ideally save freeing up orig blocks UNTIL we've called LBAWrite at the very end of this function,
        // otherwise if something goes wrong in the interim, then we've just lost all our original data
    }    

    int bytesWritten = 0; // Tracking the total number of bytes written

    // Part 1: Fill remainder of first block's buffer
    if (part1 > 0) {
        // Give part1 bytes from caller buffer
        memcpy(fcb->buf + fcb->index, buffer, part1);

        // Increment trackers
        fcb->currBlock++;
        fcb->index = 0;
        bytesWritten += part1;
    }


    // Part 2: Write multiple FULL blocks worth of data
    int blocksToWrite = part2 / vcb->blockSize;

    // Give blocksToWrite number of blocks into our file buffer
    while (blocksToWrite > 0) {
        memcpy(fcb->buf, buffer + bytesWritten, vcb->blockSize);

        // Increment trackers
        fcb->currBlock++;
        fcb->index += vcb->blockSize;
        bytesWritten += vcb->blockSize;


        blocksToWrite--;
    }

    // Part 3: Write leftover remaining data into file buffer
    // At this point we should have less than a full block of data remaining
    if (part3 > 0) {
        memcpy(fcb->buf + fcb->index, buffer + bytesWritten, part3);

        // Increment trackers
        fcb->index += part3;
        bytesWritten += count;
    }

    // Part 4: Call LBAWrite and rewrite all relevant blocks. Then FREE previous original blocks (if any)

    int totalBlocks = (totalBytes + vcb->blockSize - 1) / vcb->blockSize;
    int retVal = LBAwrite(fcb->buf, totalBlocks, fcb->position + fcb->currBlock);
    if (retVal != totalBlocks) {
        printf("\tb_write: Error, failed to write blocks. Expected: %d, got: %d\n", totalBlocks, retVal);
    }

    // TODO: Rewrite the parent directory to update any directory entries.
    // dirEntry* currDir = fcb->parentDir;
    

    if (reAlloced == 1) {

        // Free previous original blocks
        while (origBlocksAllocated > 0) {
            freeBlock(origLocation);
            
            origLocation++;
            origBlocksAllocated--;
        }

    }


    return bytesWritten; // Return the total number of bytes written

	}



// Interface to read a buffer

// Filling the callers request is broken into three parts
// Part 1 is what can be filled from the current buffer, which may or may not be enough
// Part 2 is after using what was left in our buffer there is still 1 or more block
//        size chunks needed to fill the callers request.  This represents the number of
//        bytes in multiples of the blocksize.
// Part 3 is a value less than blocksize which is what remains to copy to the callers buffer
//        after fulfilling part 1 and part 2.  This would always be filled from a refill 
//        of our buffer.
//  +-------------+------------------------------------------------+--------+
//  |             |                                                |        |
//  | filled from |  filled direct in multiples of the block size  | filled |
//  | existing    |                                                | from   |
//  | buffer      |                                                |refilled|
//  |             |                                                | buffer |
//  |             |                                                |        |
//  | Part1       |  Part 2                                        | Part3  |
//  +-------------+------------------------------------------------+--------+
int b_read (b_io_fd fd, char * buffer, int count)
	{

	if (startup == 0) b_init();  //Initialize our system

	// check that fd is between 0 and (MAXFCBS-1)
	if ((fd < 0) || (fd >= MAXFCBS))
		{
		return (-1); 					//invalid file descriptor
		}
		
	b_fcb *fcb = &fcbArray[fd];

	int bytesToRead = (count > fcb->buflen) ? fcb->buflen : count; // Here, the number of bytes is calculated.

	if (fcb->currBlock + 1 > fcb->numBlocks) // Reads from the disk
	{
		int blocksToRead = (bytesToRead > fcb->numBlocks - fcb->currBlock) ?
		fcb->numBlocks - fcb->currBlock : bytesToRead;

		char *diskBuffer = malloc(B_CHUNK_SIZE); // Now reading from disk
		if (!diskBuffer) 
		{
			return -1; // Memory allocation fails
		}

		int bytesReadFromDisk = read(fcb->position + fcb->currBlock * B_CHUNK_SIZE, diskBuffer,blocksToRead * B_CHUNK_SIZE);

		if(bytesReadFromDisk != blocksToRead * B_CHUNK_SIZE)
		{
			free(diskBuffer);
			return -1;
		}

		memcpy(diskBuffer, fcb->buf + fcb->index, bytesReadFromDisk);

		free(diskBuffer);

		fcb->buflen += bytesReadFromDisk - fcb->index;
		fcb->currBlock = 0; // After reading new data, this will reset the current block.

		if (bytesToRead > bytesReadFromDisk)
		{
			memcpy(buffer, fcb->buf + fcb->index, bytesToRead - bytesReadFromDisk);
			return bytesToRead;
		}
	}

		memcpy(buffer, fcb->buf + fcb->index, bytesToRead); // Here, data will be copied from the fcb buffer to the user buffer.

	// Updates the FCB state.
	fcb->index += bytesToRead;
	fcb->buflen -= bytesToRead;
	fcb->currBlock++;

	return bytesToRead;


	}
	
// Interface to close the file
int b_close(b_io_fd fd)
{
    // Validate the file descriptor
    if (fd < 0 || fd >= MAXFCBS || fcbArray[fd].buf == NULL) {
        return -1; // Error: invalid file descriptor
    }

    // Free allocated buffer
    free(fcbArray[fd].buf);
    fcbArray[fd].buf = NULL;

    // Reset FCB fields to default values
    fcbArray[fd].index = 0;
    fcbArray[fd].buflen = 0;
    fcbArray[fd].totalRead = 0;
    fcbArray[fd].position = 0;

    return 0; // Success
}

