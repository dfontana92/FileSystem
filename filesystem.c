/* 
	File System Internals

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "softwaredisk.h"
#include "filesystem.h"


// GLOBALS

/*
// open existing file with pathname 'name' and access mode 'mode'.  Current file
// position is set at byte 0.  Returns NULL on error. Always sets 'fserror' global.
File open_file(char *name, FileMode mode)
{
	// Find File by Name
	    // Read block with file entries
		// Scan for name - scan until find byte 0x00 (end of file entries)

		// If File Found:
		// Create FileInternal
		// Pull File Size and Initial Block Num from file entry
		// Set file position to byte 0

	// If Error, remember to set fserror global


	// Return File pointer
}

// create and open new file with pathname 'name' and access mode 'mode'.  Current file
// position is set at byte 0.  Returns NULL on error. Always sets 'fserror' global.
File create_file(char *name, FileMode mode)
{
	// Find a free block in FAT
	// Create new file entry in file entry Block
		// Can only write full block,
		//	 so read entire block via SoftwareDisk into a char(byte) buffer,
		//   
}
*/

int file_exists(char *name)
{

	//===== RETRIEVE RECORD BLOCK =====

	// Read first Disk Block to find FS Info
	//  First File Record block is located Bytes 16-19
	char* data = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	int errCheck = read_sd_block(data, 0);

		// Sanity Checks
		if(errCheck)
			printf("\nSuccessfully read Block 0 for file_exists check\n");
		else
			printf("\nFailed to read Block 0 for file exists check\n");

	// Parsing Block Index and numDirBlocks
	int firstRecordBlock;
	int numDirBlocks;
	memcpy(&firstRecordBlock, (data + 16), sizeof(int));
	memcpy(&numDirBlocks, (data + 4), sizeof(int));
	
		// Sanity Check
		printf("First Record Block: %i\nNumber of Record Blocks: %i\n", firstRecordBlock, numDirBlocks);


	// Read Record Block into buffer
	free(data);
	data = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	errCheck = read_sd_block(data, firstRecordBlock);

	//===== RECORD CHECKING ======

	// Calculate name length
	//  length/23 number of records required to match
	int nameLength = strlen(name);
	int numRecords = (int)ceil(nameLength/23);

	// Loop Indexing
	int alignment = 32;
	int entryIndex = 0;
	int entryOffset; 		// will be alignment * entryIndex  for loop

	// !!! MAKE THIS LOOP (until first byte of entry = 0x00)
	// Check if record present 				(0th bit)
	//  then check if parent				(1st bit)
	//  then check if numRecords is correct (5-7 bit)
	//  then check name -- byte(char) 5-7
	
	int maxRecords = numDirBlocks * 512 / 32;

	for(int i = 0; i < maxRecords; i++)
	{
		// Get fileAttr byte
		unsigned char fileAttr;
		memcpy(&fileAttr, data, sizeof(char));

		if(fileAttr == 0) // End of Records
			break;

		if(isNthBitSet(fileAttr, 0)) // is Record Present
		{
			if(isNthBitSet(fileAttr, 1)) // is Parent Record
			{
				// Check numRecords against 
			}
		} 
	}
	
	/* THIS PASSED PREVIOUSLY
	if(isNthBitSet(fileAttr, 0))
	{
		printf("Detected File Record\n");
		
		// !!! NEED TO HANDLE LONG FILE NAMES
		//  Probably get strlen from name arg
		//  then use length/23 to determine number of records
		//  if 3 least-sig bits don't match that number then skip to next parent
		//  if number of records matches, then read name (below)
		//   Must scale fileName[] to length
		//   			memcpy(d, s, length*sizeof(char))


		// Check Name
		char fileName[23];
		memcpy(fileName, data+9, 23*sizeof(char));

		printf("File Name: %s\n", fileName);
		printf("Searching for: %s\n", name);

		if(!strcmp(fileName, name))
			printf("Test Success!\n");
	}
	*/


	return 1;
}

void iEndianSwap(int* num)
{
	*num = ((*num>>24)&0xff) |	 	// move byte 3 to byte 0
			((*num<<8)&0xff0000) | 	// move byte 1 to byte 2
    		((*num>>8)&0xff00) | 	// move byte 2 to byte 1
    		((*num<<24)&0xff000000); // byte 0 to byte 3
}

int isNthBitSet (unsigned char c, int n) {
    static unsigned char mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
    return ((c & mask[n]) != 0);
}