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
FSError Error = FS_NONE;

typedef struct FileInternals 
{
	unsigned int recordNumber;
    unsigned int fileSize;
    unsigned int absFilePos;
    unsigned int relFilePos;
    unsigned int startingBlock;
    unsigned int currentBlock;
    FileMode mode;
} FileInternals; 

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
*/

// create and open new file with pathname 'name' and access mode 'mode'.  Current file
// position is set at byte 0.  Returns NULL on error. Always sets 'fserror' global.
File create_file(char *name, FileMode mode)
{
	Error = FS_NONE;

	// Check IF file already exists
	if(file_exists(name))
	{
		Error = FS_FILE_ALREADY_EXISTS;
		return NULL;
	}

	// Get number of records needed to create file
	unsigned int length = strlen(name);
	unsigned int recordsRequired = (int)ceil(length/23.0);

	// Find unused data block
	unsigned int firstBlock = get_free_data_block();
	if(Error == FS_OUT_OF_SPACE)
		return NULL;
	
	// Find suitable file record
	unsigned int firstRecord = get_free_record(recordsRequired);
	if (Error == FS_OUT_OF_SPACE)
		return NULL;




	return NULL;
}

// determines if a file with 'name' exists and returns 1 if it exists, otherwise 0.
// Always sets 'fserror' global.
int file_exists(char *name)
{

	Error = FS_NONE;

	// ========== RETRIEVE RECORD BLOCK ===========
	// ============================================

	// First File Record block is located Bytes 16-19
	char* data = calloc(512, sizeof(char));
	int errCheck = read_sd_block(data, 0);

	// Parsing Block Index and numDirBlocks
	int firstRecordBlock;
	int numDirBlocks;
	memcpy(&firstRecordBlock, (data + 16), sizeof(int));
	memcpy(&numDirBlocks, (data + 4), sizeof(int));

	// Read File Record Block
	free(data);
	data = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	read_sd_block(data, firstRecordBlock);


	// ========== RECORD CHECKING ===========
	// ======================================

	unsigned int nameLength = strlen(name);
	unsigned char numRecordsTarget = (char)ceil(nameLength/23.0);


	// Loop Info: scanning File Records 	(!!! Need to have case for overflow to next File Record block !!!)
	// Check if record present 				(0th bit of fileAttr)
	// .check if parent						(1st bit of fileAttr)
	// ..check if numRecords is correct 	(4-7 bits of fileAttr)
	// ...check name (inner loop)			(bytes 9-31)

	unsigned int alignment = 32;
	unsigned int maxRecords = numDirBlocks * 512 / 32;

	for(int entryIndex = 0; entryIndex < maxRecords; entryIndex++)
	{
		// Byte-offset from beginning of block to beginning of current entry
		unsigned int entryOffset = alignment * entryIndex;

		// fileAttr byte
		//  0 		- exists
		//  1 		- parent
		//  2 		- in use
		//  3 		- n/a
		//  [4,7] 	- internal entry index (total entries if parent)
		unsigned char fileAttr;
		memcpy(&fileAttr, data + entryOffset, sizeof(char));

		if(fileAttr == 0) // End of Records
		{
			printf("\nLast Record: %i\n", entryIndex-1);
			break;
		}

		if(isNthBitSet(fileAttr, 0)) // is Record Present
		{
			printf("\nRecord %i Present\n", entryIndex);
			if(isNthBitSet(fileAttr, 1)) // is Parent Record
			{
				printf(".Record %i is Parent\n", entryIndex);
				unsigned char numRecordsCurrent = fileAttr & 15;
				if(numRecordsCurrent == numRecordsTarget) // Matches record length
				{
					printf("..Found present, parent record of matching length (%i)\n", numRecordsCurrent);
					
					// Name Checking
					unsigned char fileName[numRecordsCurrent*23];

					// Loop to read full name from multiple File Records
					for(int internalIndex = 0; internalIndex < numRecordsCurrent; internalIndex++)
					{
						// Offset 
						// Name is bytes 9-31 of each record (23 Bytes)
						printf("...Copying entry %i name\n", internalIndex+1);
						memcpy((fileName + (internalIndex*23)), (data + entryOffset + (internalIndex*32) + 9), 23*sizeof(char));
					}

					printf("Found Name: %s\n", fileName);
					printf("Searching for: %s\n", name);

					if(!strcmp(fileName, name))
					{
						// FILE FOUND
						free(data);
						return 1;
					}
				}
			}
		}
	}

	// FILE NOT FOUND
	free(data);
	return 0; 
}

// describe current filesystem error code by printing a descriptive message to standard
// error.
void fs_print_error(void)
{
	if(Error == FS_NONE)
		fprintf(stderr, "Operation Successful - No Error.\n");

	if(Error == FS_FILE_ALREADY_EXISTS)
		fprintf(stderr, "Operation Failed - File Already Exists.\n");
}

// Allocates a data block, updating parent's FAT value
unsigned int allocate_data_block(int* parentFatIndexPtr, int targetFatIndex)
{
	#define firstFatBlock info.firstFatBlock

	struct FSInfo info = get_fs_info();

	unsigned int entriesPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_FAT_ENTRY;
	unsigned int targetFatBlockNumber = targetFatIndex / entriesPerBlock;
	unsigned int targetInternalIndex = targetFatIndex - (targetFatBlockNumber * entriesPerBlock);

	// Read FAT Block containing the Target Entry
	char* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	read_sd_block(blockData, (targetFatBlockNumber + firstFatBlock));

	// Get Current Value of Target Entry, check it is free
	unsigned int currentValue;
	memcpy(&currentValue, (blockData + (targetInternalIndex * SIZE_OF_FAT_ENTRY)), sizeof(int));

		// Block is currently allocated (ERROR)
		if(currentValue != 0x00000000)
		{
			printf("Internal FileSystem Error - Allocation Failed - Block Already Allocated\n");
			free(blockData);
			return 0;
		}

	// Write termination symbol to complete allocation
	currentValue = 0xFFFFFFFF;
	memcpy((blockData + (targetInternalIndex * SIZE_OF_FAT_ENTRY)), &currentValue, sizeof(int));
	write_sd_block(blockData, (targetFatBlockNumber +firstFatBlock));

	// Regular Allocation Case (WRITE, SEEK), Must Update Parent
	if(parentFatIndexPtr != NULL)
	{
		unsigned int parentFatIndex = *parentFatIndexPtr;
		unsigned int parentFatBlockNumber = parentFatIndex / entriesPerBlock;
		unsigned int parentInternalIndex = parentFatIndex - (parentFatBlockNumber * entriesPerBlock);

		free(blockData);

		blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
		read_sd_block(blockData, (parentFatBlockNumber + firstFatBlock));

		memcpy(&currentValue, (blockData + (parentInternalIndex * SIZE_OF_FAT_ENTRY)), sizeof(int));

			if(currentValue != 0xFFFFFFFF)
			{
				printf("Internal FileSystem Error - Allocation Failed - Parent NOT End of Chain");
				free(blockData);
				return 0;
			}

		currentValue = targetFatIndex;
		memcpy((blockData + (parentInternalIndex * SIZE_OF_FAT_ENTRY)), &currentValue, sizeof(int));
	}

	free(blockData);
	return 1;
	#undef firstFatBlock
}

// Writes a val into FAT entryNumber
/*
unsigned int write_fat_entry(unsigned int entryNumber, unsigned int entryVal)
{
	#define firstFatBlock info.firstFatBlock

	struct FSInfo info = get_fs_info();

	unsigned int entryPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_FAT_ENTRY;
	unsigned int fatBlockNumber = entryNumber / entryPerBlock;
	unsigned int internalIndex = entryNumber - (fatBlockNumber * entryPerBlock);

	int* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	read_sd_block(blockData, (fatBlockNumber + firstFatBlock));

	// Writing entryVal to the entry using index (pointer-arithmetic is integer)
	memcpy((blockData + internalIndex), &entryVal, sizeof(int));
	int success = write_sd_block(blockData, (fatBlockNumber + firstFatBlock));

	free(blockData);

	if(success)
		printf("Successfully wrote %i to FAT entry %i\n", entryVal, entryNumber);

	#undef firstFatBlock
}
*/


unsigned int write_record_entry()
{

}

unsigned int update_file_size(unsigned int recordNumber, unsigned int size)
{
	#define firstRecordBlock info.firstRecordBlock

	struct FSInfo info = get_fs_info();

	unsigned int recordsPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_RECORD_ENTRY;
	unsigned int dirBlockNumber = recordNumber / recordsPerBlock;
	unsigned int internalIndex = recordNumber - (dirBlockNumber * recordsPerBlock);

	char* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	read_sd_block(blockData, (dirBlockNumber + firstRecordBlock));

	// Writing size to entry using index and 5-byte offset into entry (pointer-arithmetic is char)
	memcpy(blockData + (internalIndex * SIZE_OF_RECORD_ENTRY) + 5, &size, sizeof(int));
	int success = write_sd_block(blockData, (dirBlockNumber + firstRecordBlock));

	free(blockData);

	if(success)
		printf("Successfully wrote %i file size to file record %i\n", size, recordNumber);

	#undef firstRecordBlock
}

// Finds and Returns the data Block Number of the first free Data Block
//  ~ This is the FAT entry number
//  ~~ Must offset by +firstDataBlock to read/write block
unsigned int get_free_data_block()
{
	FSInfo info = get_fs_info();

	#define firstFatBlock info.firstFatBlock
	#define numFatBlocks info.numFatBlocks
	#define firstDataBlock info.firstDataBlock

	// Don't read past maxFatRecords
	unsigned int entriesPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_FAT_ENTRY;
	unsigned int maxFatRecords = numFatBlocks * entriesPerBlock;
	char* blockData;

	for(unsigned int blockIndex = 0; blockIndex < numFatBlocks; blockIndex++)
	{

		// Read Current FAT Block
		blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
		read_sd_block(blockData, firstFatBlock+blockIndex);

		for(unsigned int entryIndex = 0; entryIndex < entriesPerBlock; entryIndex++)
		{
			unsigned int entryVal;
			memcpy(&entryVal, (blockData + (SIZE_OF_FAT_ENTRY * entryIndex)), sizeof(int));

			// Found Free FAT Entry
			if(entryVal == 0)
			{
				free(blockData);
				return (blockIndex * entriesPerBlock + entryIndex);
			}
		}

		free(blockData);
	}

	// No Free Blocks
	Error = FS_OUT_OF_SPACE;

	#undef firstFatBlock
	#undef numFatBlocks
	#undef firstDataBlock
}

// Finds and Returns the Record Number of the first free contiguous Record entries of length
unsigned int get_free_record(unsigned int length)
{
	#define firstRecordBlock info.firstRecordBlock
	#define numRecordBlocks info.numRecordBlocks

	struct FSInfo info = get_fs_info();

	unsigned int entriesPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_RECORD_ENTRY;
	unsigned int maxRecordEntries = numRecordBlocks * entriesPerBlock;
	char* blockData;

	for(unsigned int blockIndex = 0; blockIndex < numRecordBlocks; blockIndex++)
	{

		// Read current Record Block
		blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
		read_sd_block(blockData, (firstRecordBlock + blockIndex));

		unsigned int counter = 0;

		for(int entryIndex = 0; entryIndex < entriesPerBlock; entryIndex++)
		{
			unsigned char fileAttr;
			memcpy(&fileAttr, blockData + (SIZE_OF_RECORD_ENTRY * entryIndex), sizeof(char));

			if(fileAttr == 0)
			{
				counter++;

				if(counter == length)
				{
					free(blockData);
					return ((blockIndex * entriesPerBlock + entryIndex) - (length - 1));
				}
			}
			else
			{
				counter = 0;
			}
		}
	}

	// No Free Records of suitable size
	Error = FS_OUT_OF_SPACE;

	#undef firstRecordBlock
	#undef numRecordBlocks

}

struct FSInfo get_fs_info()
{
	// Block 0
	char* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	read_sd_block(blockData, 0);

	struct FSInfo info;
	unsigned int offset = 0;

	memcpy(&info.numFatBlocks, blockData, sizeof(int));
		offset += sizeof(int);
	memcpy(&info.numRecordBlocks, blockData + offset, sizeof(int));
		offset += sizeof(int);
	memcpy(&info.numDataBlocks, blockData + offset, sizeof(int));
		offset += sizeof(int);
	memcpy(&info.firstFatBlock, blockData + offset, sizeof(int));
		offset += sizeof(int);
	memcpy(&info.firstRecordBlock, blockData + offset, sizeof(int));
		offset += sizeof(int);
	memcpy(&info.firstDataBlock, blockData + offset, sizeof(int));
		offset += sizeof(int);
	memcpy(&info.lastUsedBlock, blockData + offset, sizeof(int));
		offset += sizeof(int);

	free(blockData);

	return info;
}

// May Be Unneeded Now
void iEndianSwap(int* num)
{
	*num = ((*num>>24)&0xff) |	 	// move byte 3 to byte 0
			((*num<<8)&0xff0000) | 	// move byte 1 to byte 2
    		((*num>>8)&0xff00) | 	// move byte 2 to byte 1
    		((*num<<24)&0xff000000); // byte 0 to byte 3
}

// Checks if Nth bit of Byte c is set
int isNthBitSet (unsigned char c, int n) {
    static unsigned char mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
    return ((c & mask[n]) != 0);
}