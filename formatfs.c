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

	// Write FileSys Info to Block 0
	//	numFatBlocks
	//  numDirBlocks
	//  numDataBlocks
	//	firstFatBlock
	//	firstDirBlock
	//  firstDataBlock

	char* data = malloc(blockSize*sizeof(char));

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

	// Passes
	char* input = malloc(blockSize*sizeof(char));
	temp = read_sd_block(input, 0);
	printf("Read from Block 0 returned %i\n", temp);
}