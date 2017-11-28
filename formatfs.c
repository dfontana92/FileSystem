/*
	Gotta Format that FS, baby
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "softwaredisk.h"

int main()
{
	// Initialize the Software Disk, yo
	// !!! Add error handling to this
	init_software_disk();

	// Now layout that sweet disk structure
	int blockSize = SOFTWARE_DISK_BLOCK_SIZE;
	int numBlocks = software_disk_size();

	printf("Disk Size is %i Blocks / %i Bytes\n", numBlocks, (numBlocks*blockSize));

	printf("First Block is FileSys Info\n");

	// FAT using 4-byte (32-bit) integers for Block allocation numbers
	int numFatBlocks = (int)ceil((4.0 * numBlocks / blockSize));
	printf("FAT using %i Blocks\n", numFatBlocks);

	// Use 1% of Disk Space for file entries
	int numDirBlocks = (int)ceil((numBlocks * 0.01));
	printf("File Entries using %i Blocks\n", numDirBlocks);

	// Rest of disk is data
	int numDataBlocks = numBlocks - 1 - numFatBlocks - numDirBlocks;
	printf("Data using remaining %i Blocks\n", numDataBlocks);

	// Offsets
	int firstFatBlock = 1;
	int firstDirBlock = 1 + numFatBlocks;
	int firstDataBlock = 1 + numFatBlocks + numDirBlocks;

	// Free Space Tracker (not super efficient)
	int lastUsedBlock = 0;

	// Write FileSys Info to Block 0
	//	numFatBlocks	(bytes 0-3)
	//  numDirBlocks	(bytes 4-7)
	//  numDataBlocks	(bytes 8-11)
	//	firstFatBlock	(bytes 12-15)
	//	firstDirBlock	(bytes 16-19)
	//  firstDataBlock	(bytes 20-23)
	//  lastUsedBlock	(bytes 24-27) - starts as 0, no need to write

		char* data = calloc(blockSize, sizeof(char));

		int offset = 0;

		memcpy(data + offset, &numFatBlocks, sizeof(numFatBlocks));
		offset += sizeof(numFatBlocks);

		memcpy(data + offset, &numDirBlocks, sizeof(numDirBlocks));
		offset += sizeof(numDirBlocks);

		memcpy(data + offset, &numDataBlocks, sizeof(numDataBlocks));
		offset += sizeof(numDataBlocks);

		memcpy(data + offset, &firstFatBlock, sizeof(firstFatBlock));
		offset += sizeof(firstFatBlock);

		memcpy(data + offset, &firstDirBlock, sizeof(firstDirBlock));
		offset += sizeof(firstDirBlock);

		memcpy(data + offset, &firstDataBlock, sizeof(firstDataBlock));
		offset += sizeof(firstDataBlock);

		// Passes
		int temp = write_sd_block((void*)data, 0);
		printf("\nWrite to Block 0 returned %i\n", temp);

		// Cleanup
		free(data);

		/* Passes
		char* input = calloc(blockSize, sizeof(char));
		temp = read_sd_block(input, 0);
		printf("Read from Block 0 returned %i\n", temp);
		*/

	// Write initial FAT table
	//  Basically just 0x00 to all bytes (so don't write anything..)

	// Write initial FileRecord
	//  Again basically just 0x00 for all bytes sooo...

	// !!! Testing File Record
	char fileAttr = 0b10000001;
	int dataBlock = 0;
	int fileSize = 300;
	char name[23] = "Little";

	data = calloc(blockSize, sizeof(char));
	memcpy(data, &fileAttr, sizeof(char));
	memcpy(data+1, &dataBlock, sizeof(int));
	memcpy(data+5, &fileSize, sizeof(int));
	memcpy(data+9, name, sizeof(name));

	write_sd_block((void*)data, firstDirBlock);



	printf("File System Initialization Successful");
}