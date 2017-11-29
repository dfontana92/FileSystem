/*
	** 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "softwaredisk.h"

int main()
{
	init_software_disk();

	int blockSize = SOFTWARE_DISK_BLOCK_SIZE;
	int numBlocks = software_disk_size();


	// FAT using 4-byte (32-bit) integers for Block allocation numbers
	int numFatBlocks = (int)ceil((4.0 * numBlocks / blockSize));

	// Use 1% of Disk Space for file entries
	int numRecordBlocks = (int)ceil((numBlocks * 0.01));

	// Rest of disk is data
	int numDataBlocks = numBlocks - 1 - numFatBlocks - numRecordBlocks;

	// Offsets
	int firstFatBlock = 1;
	int firstRecordBlock = 1 + numFatBlocks;
	int firstDataBlock = 1 + numFatBlocks + numRecordBlocks;

	// Free Space Tracker (not super efficient)
	int lastUsedBlock = 0;

	// Write FileSys Info to Block 0
	//	numFatBlocks	(bytes 0-3)
	//  numDirBlocks	(bytes 4-7)
	//  numDataBlocks	(bytes 8-11)
	//	firstFatBlock	(bytes 12-15)
	//	firstRecordBlock(bytes 16-19)
	//  firstDataBlock	(bytes 20-23)
	//  lastUsedBlock	(bytes 24-27) - starts as 0, no need to write


		char* data = calloc(blockSize, sizeof(char));

		int offset = 0;

		memcpy(data + offset, &numFatBlocks, sizeof(numFatBlocks));
		offset += sizeof(numFatBlocks);

		memcpy(data + offset, &numRecordBlocks, sizeof(numRecordBlocks));
		offset += sizeof(numRecordBlocks);

		memcpy(data + offset, &numDataBlocks, sizeof(numDataBlocks));
		offset += sizeof(numDataBlocks);

		memcpy(data + offset, &firstFatBlock, sizeof(firstFatBlock));
		offset += sizeof(firstFatBlock);

		memcpy(data + offset, &firstRecordBlock, sizeof(firstRecordBlock));
		offset += sizeof(firstRecordBlock);

		memcpy(data + offset, &firstDataBlock, sizeof(firstDataBlock));
		offset += sizeof(firstDataBlock);

		// Passes
		write_sd_block((void*)data, 0);

		// Cleanup
		free(data);

	//printf("File System Initialization Completed\n");
}