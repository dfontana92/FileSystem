/* 
	File System Internals
	
	*** MUST BE COMPILED WITH -lm FLAG ***

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "softwaredisk.h"
#include "filesystem.h"

// GLOBALS
FSError Error;

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

	// Find Free Data Block
	unsigned int firstBlock = get_free_data_block();
	if(Error == FS_OUT_OF_SPACE)
		return NULL;

	// Write File Record (sets Error if error)
	unsigned int recordIndex = write_record_entry(name, firstBlock);
	if(Error == FS_OUT_OF_SPACE)
		return NULL;

	// Allocate Data Block
	allocate_data_block(NULL, firstBlock);


	// Construct the FileInternals
	FileInternals* f = malloc(sizeof(FileInternals));

	(*f).recordNumber = recordIndex;
	(*f).fileSize = 0;
	(*f).filePos = 0;
	(*f).startingBlock = firstBlock;
	(*f).currentBlock = firstBlock;
	(*f).mode = mode;

	// Success!
	return f;
}

// open existing file with pathname 'name' and access mode 'mode'.  Current file
// position is set at byte 0.  Returns NULL on error. Always sets 'fserror' global.
File open_file(char *name, FileMode mode)
{
	Error = FS_NONE;

	#define numRecordBlocks info.numRecordBlocks
	#define firstRecordBlock info.firstRecordBlock

	FSInfo info = get_fs_info();

	char* blockData;

	// ========== RECORD CHECKING ==========
	// =====================================

	unsigned int nameLength = strlen(name);
	unsigned char recordsRequired = (char)ceil(nameLength/23.0);

	unsigned int recordsPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_RECORD_ENTRY;

	for(unsigned int blockIndex = 0; blockIndex < numRecordBlocks; blockIndex++)
	{
		unsigned int absBlockNumber = blockIndex + firstRecordBlock;

		blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
		read_sd_block(blockData, absBlockNumber);

		for(unsigned int recordIndex = 0; recordIndex < recordsPerBlock; recordIndex++)
		{
			unsigned int entryOffset = recordIndex * SIZE_OF_RECORD_ENTRY;

			unsigned char fileAttr;
			memcpy(&fileAttr, blockData + entryOffset, sizeof(char));

			// END OF RECORDS, FILE NOT FOUND
			if(fileAttr == 0)
			{
				//printf("File Not Found: %s\n", name);
				Error = FS_FILE_NOT_FOUND;
				return NULL;
			}

			// IF RECORD PRESENT
			if(isNthBitSet(fileAttr, 0))
			{
				// IF RECORD IS PARENT
				if(isNthBitSet(fileAttr, 1))
				{
					unsigned char numRecordsCurrent = fileAttr & 15;
					if(numRecordsCurrent == recordsRequired)
					{
						unsigned char fileName[numRecordsCurrent*23];

						for(unsigned int internalIndex = 0; internalIndex < numRecordsCurrent; internalIndex++)
						{
							memcpy((fileName + (internalIndex*23)), (blockData + entryOffset + (internalIndex*32) + 9), 23*sizeof(char));
						}

						// IF NAMES MATCH, FILE FOUND
						if(!strcmp(fileName, name))
						{
							// IF FILE IS OPEN
							if(isNthBitSet(fileAttr, 2))
							{
								Error = FS_FILE_OPEN;
								printf("File Already Open: %s\n", name);
								return NULL;
							}


							// Read Record Information into local vars
							unsigned int firstBlock;
							memcpy(&firstBlock, blockData + entryOffset + 1, sizeof(int));

							unsigned int fileSize;
							memcpy(&fileSize, blockData + entryOffset + 5, sizeof(int));

							// Set Open Flag on this record
							fileAttr |= 32;
							memcpy(blockData + entryOffset, &fileAttr, sizeof(char));
							write_sd_block(blockData, absBlockNumber);

							// CONSTRUCT FILEINTERNALS
							FileInternals* f = malloc(sizeof(FileInternals));

							(*f).recordNumber = recordIndex + (blockIndex * recordsPerBlock);
							(*f).fileSize = fileSize;
							(*f).filePos = 0;
							(*f).startingBlock = firstBlock;
							(*f).currentBlock = firstBlock;
							(*f).mode = mode;

							// Return FileInternal
							printf("File Opened: %s\n", name);
							return f;
						}
					}
				}
			}
		}
	
		// FREE THE BUFFER!
		free(blockData);
	}

	#undef numRecordBlocks
	#undef firstRecordBlock
}

// close 'file'.  Always sets 'fserror' global.
void close_file(File file)
{
	Error = FS_NONE;

	#define firstRecordBlock info.firstRecordBlock
	FSInfo info = get_fs_info();

	unsigned int recordNumber = (*file).recordNumber;

	unsigned int recordsPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_RECORD_ENTRY;
	unsigned int blockIndex = recordNumber / recordsPerBlock;
	unsigned int recordIndex = recordNumber - (blockIndex * recordsPerBlock);
	unsigned int recordOffset = recordIndex * SIZE_OF_RECORD_ENTRY;

	// Read Block with recordNumber in it
	char* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	read_sd_block(blockData, blockIndex + firstRecordBlock);

	unsigned char fileAttr;
	memcpy(&fileAttr, blockData + recordOffset, sizeof(char));

	// IF FILE IS OPEN
	if(isNthBitSet(fileAttr, 2))
	{
		fileAttr = fileAttr & ~(32);
		memcpy(blockData + recordOffset, &fileAttr, sizeof(char));

		write_sd_block(blockData, blockIndex + firstRecordBlock);
	}
	else
	{
		Error = FS_FILE_NOT_OPEN;
	}

	free(blockData);
	#undef firstRecordBlock
}

// read at most 'numbytes' of data from 'file' into 'buf', starting at the 
// current file position.  Returns the number of bytes read. If end of file is reached,
// then a return value less than 'numbytes' signals this condition. Always sets
// 'fserror' global.
unsigned long read_file(File file, void *buf, unsigned long numbytes)
{
	#define firstDataBlock info.firstDataBlock

	FSInfo info = get_fs_info();


	Error = FS_NONE;

	unsigned int recordNumber = (*file).recordNumber;

	if(is_open(recordNumber) == 0)
	{
		Error = FS_FILE_NOT_OPEN;
		return 0;
	}

	// IF READ REQUEST IS BIGGER THAN FILE
	//  READ TO END OF FILE.
	if((*file).fileSize < ((*file).filePos + numbytes));
		numbytes = (*file).fileSize - (*file).filePos;

	// Position in Current Block
	unsigned int relativePos = (*file).filePos % SOFTWARE_DISK_BLOCK_SIZE;

	unsigned int bytesRead = 0;

	char* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));

	// READ CURRENT DATA BLOCK
	unsigned int currentBlockIndex = (*file).currentBlock;
	read_sd_block(blockData, currentBlockIndex + firstDataBlock);

	// NEXT BLOCK VALUE
	unsigned int nextBlock = get_next_data_block(currentBlockIndex);

	// More than 1 Read?
	int readOverflow = numbytes - (SOFTWARE_DISK_BLOCK_SIZE - relativePos);

	// SINGLE READ
	if(readOverflow <= 0) {
		// COPY TO BUF, FROM BLOCKDATA+RELATIVEPOS, NUMBYTES
		memcpy(buf, blockData + relativePos, numbytes);

		// UPDATE BYTES WRITTEN
		bytesRead += numbytes;
	}
	// MULTIPLE READS
	else
	{
		unsigned int numReads = (int)ceil(readOverflow / SOFTWARE_DISK_BLOCK_SIZE);

		// ========== FIRST READ ==========
		// =================================
			// COPY TO BUF, FROM BLOCKDATA+RELATIVEPOS, (SDBS - RELATIVEPOS) BYTES
			memcpy(buf, blockData + relativePos, (SOFTWARE_DISK_BLOCK_SIZE - relativePos));

			// UPDATE BYTES WRITTEN
			bytesRead += (SOFTWARE_DISK_BLOCK_SIZE - relativePos);


		// =========== LOOPING READ ===========
		// =====================================
			for(int i = 1; i < numReads; i++)
			{

			// ========== CONTEXT SWITCHING ==========
			// =======================================
				// IS END-OF-FILE
				if(nextBlock == 0xFFFFFFFF)
				{
					// DO NOT READ PAST EOF
					break;
				}
				// NOT END-OF-FILE
				else
				{
					// SWITCH CONTEXT TO NEXT BLOCK
					currentBlockIndex = nextBlock;
					nextBlock = get_next_data_block(currentBlockIndex);
				}

			// ========== DATA READING ==========
			// ==================================
				// READ NEW DATA BLOCK INTO BUFFER
				read_sd_block(blockData, currentBlockIndex + firstDataBlock);

					// MORE THAN FULL BLOCK LEFT
					if((numbytes - bytesRead) > SOFTWARE_DISK_BLOCK_SIZE)
					{
						// COPY SW_D_BLK_SIZE TO BUF
						memcpy(buf, blockData, SOFTWARE_DISK_BLOCK_SIZE);

						// INCREMENT BYTES WRITTEN
						bytesRead += SOFTWARE_DISK_BLOCK_SIZE;
					}
					// LESS THAN FULL BLOCK LEFT
					else
					{
						// COPY (NUMBYTES - BYTESWRITTEN) TO BUFFER
						memcpy(buf, blockData, (numbytes - bytesRead));

						// INCREMENT BYTES WRITTEN
						bytesRead += (numbytes - bytesRead);
					}
			}
	}

	free(blockData);

	(*file).filePos += bytesRead;

	return bytesRead;

	#undef firstDataBlock
}

// write 'numbytes' of data from 'buf' into 'file' at the current file position. 
// Returns the number of bytes written. On an out of space error, the return value may be
// less than 'numbytes'.  Always sets 'fserror' global.
unsigned long write_file(File file, void *buf, unsigned long numbytes)
{
	Error = FS_NONE;

	if((*file).mode == READ_ONLY)
	{
		Error = FS_FILE_READ_ONLY;
		return 0;
	}

	#define firstDataBlock info.firstDataBlock
	#define firstRecordBlock info.firstRecordBlock
	#define firstFatBlock info.firstFatBlock

	FSInfo info = get_fs_info();

	unsigned int recordNumber = (*file).recordNumber;

	// ======== GET METADATA =========
	// ===============================

		// READ FILE RECORD BLOCK
		unsigned int recordsPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_RECORD_ENTRY;
		unsigned int blockIndex = recordNumber / recordsPerBlock;
		unsigned int recordIndex = recordNumber - (blockIndex * recordsPerBlock);
		unsigned int recordOffset = recordIndex * SIZE_OF_RECORD_ENTRY;

		char* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
		read_sd_block(blockData, blockIndex + firstRecordBlock);

		// GET FILE ATTRIBUTES
		unsigned int fileAttr;
		memcpy(&fileAttr, blockData + recordOffset, sizeof(char));

		// IF -- FILE IS NOT OPEN
		if(!isNthBitSet(fileAttr, 2))
		{
			Error = FS_FILE_NOT_OPEN;
			return 0;
		}

	// Position in Current Block
	unsigned int relativePos = (*file).filePos % SOFTWARE_DISK_BLOCK_SIZE;

	unsigned int bytesWritten = 0;

	// READ CURRENT DATA BLOCK
	unsigned int currentBlockIndex = (*file).currentBlock;
	read_sd_block(blockData, currentBlockIndex + firstDataBlock);

	// NEXT BLOCK VALUE
	unsigned int currentVal = get_next_data_block(currentBlockIndex);

	// More than 1 Write?
	int writeOverflow = numbytes - (SOFTWARE_DISK_BLOCK_SIZE - relativePos);

	// SINGLE WRITE
	if(writeOverflow <= 0) {
		// COPY TO BLOCKDATA+RELATIVEPOS, FROM *BUF, NUMBYTES
		memcpy(blockData + relativePos, buf, numbytes);

		// WRITE THE BLOCK
		write_sd_block(blockData, currentBlockIndex + firstDataBlock);

		// UPDATE BYTES WRITTEN
		bytesWritten += numbytes;
	}
	// MULTIPLE WRITES
	else {
	
		unsigned int numWrites = (int)ceil(writeOverflow / SOFTWARE_DISK_BLOCK_SIZE);

		// ========== FIRST WRITE ==========
		// =================================
			// COPY TO BLOCKDATA+RELATIVEPOS, FROM *BUF, (SDBS - RELATIVEPOS) BYTES
			memcpy(blockData + relativePos, buf, (SOFTWARE_DISK_BLOCK_SIZE - relativePos));

			// WRITE THE BLOCK
			write_sd_block(blockData, currentBlockIndex + firstDataBlock);

			// UPDATE BYTES WRITTEN
			bytesWritten += (SOFTWARE_DISK_BLOCK_SIZE - relativePos);


		// =========== LOOPING WRITE ===========
		// =====================================
			for(int i = 1; i < numWrites; i++)
			{

			// ========== CONTEXT SWITCHING ==========
			// =======================================
				// IS END-OF-FILE
				if(currentVal == 0xFFFFFFFF)
				{
					// ATTEMPT TO FIND A FREE BLOCK
					unsigned int nextBlockIndex = get_free_data_block();
						if(nextBlockIndex = 0xFFFFFFFF)
						{
							Error = FS_OUT_OF_SPACE;
							break;
						}

					// ATTEMPT TO ALLOCATE A DATA BLOCK WITH CURRENTBLOCK AS PARENT
					unsigned int success = allocate_data_block(&currentBlockIndex, nextBlockIndex);
						if(success != 1)
						{
							printf("Internal FileSystem Error - Failed to Allocate Next Block\n");
							break;
						}

					// SWITCH CURRENTBLOCK CONTEXT TO NEWLY ALLOCATED BLOCK
					currentBlockIndex = nextBlockIndex;
					currentVal = get_next_data_block(currentBlockIndex);
				}
				// NOT END-OF-FILE
				else
				{
					// SWITCH CONTEXT TO NEXT BLOCK
					currentBlockIndex = currentVal;
					currentVal = get_next_data_block(currentBlockIndex);
				}

			// ========== DATA WRITING ==========
			// ==================================
				// READ NEW DATA BLOCK INTO BUFFER
				read_sd_block(blockData, currentBlockIndex + firstDataBlock);

					// MORE THAN FULL BLOCK LEFT
					if((numbytes - bytesWritten) > SOFTWARE_DISK_BLOCK_SIZE)
					{
						// COPY SW_D_BLK_SIZE TO BUFFER
						memcpy(blockData, buf, SOFTWARE_DISK_BLOCK_SIZE);

						// WRITE BLOCK
						write_sd_block(blockData, currentBlockIndex + firstDataBlock);

						// INCREMENT BYTES WRITTEN
						bytesWritten += SOFTWARE_DISK_BLOCK_SIZE;
					}
					// LESS THAN FULL BLOCK LEFT
					else
					{
						// COPY (NUMBYTES - BYTESWRITTEN) TO BUFFER
						memcpy(blockData, buf, (numbytes - bytesWritten));

						// WRITE BLOCK
						write_sd_block(blockData, currentBlockIndex + firstDataBlock);

						// INCREMENT BYTES WRITTEN
						bytesWritten += (numbytes - bytesWritten);
					}
			}

	}

	// CHECK FOR FILE SIZE INCREASE
	if(((*file).filePos + bytesWritten) > (*file).fileSize)
	{
		(*file).fileSize = (*file).filePos + bytesWritten;
		update_file_size(recordNumber, (*file).fileSize);
	}

	(*file).filePos += bytesWritten;

	free(blockData);
	
	return bytesWritten;

	#undef firstDataBlock
	#undef firstRecordBlock
	#undef firstFatBlock
}

// sets current position in file to 'bytepos', always relative to the beginning of file.
// Seeks past the current end of file should extend the file. Always sets 'fserror'
// global.
void seek_file(File file, unsigned long bytepos)
{
	Error = FS_NONE;
	unsigned int recordNumber = (*file).recordNumber;

	if(is_open(recordNumber) == 0)
	{
		Error = FS_FILE_NOT_OPEN;
		return;
	}

	unsigned int fileSize = (*file).fileSize;

	unsigned int startingBlock = (*file).startingBlock;
	unsigned int currentBlock = startingBlock;

	// How many blocks we are seeking into
	unsigned int numBlocks = bytepos / SOFTWARE_DISK_BLOCK_SIZE;

	// Offset into last block
	unsigned int byteOffset = bytepos - (numBlocks * SOFTWARE_DISK_BLOCK_SIZE);

	// LOOP TO GET CURRENT BLOCK
	for(int i = 0; i < numBlocks; i++)
	{
		// GET CHILD BLOCK
		unsigned int next = get_next_data_block(currentBlock);

		// IF CHILD 0XFFFFFFFF (EOF)
		if(next == 0XFFFFFFFF)
		{
			// FIND NEXT FREE BLOCK
			unsigned int freeBlock = get_free_data_block();
				if(freeBlock == 0xFFFFFFFF)
				{
					Error = FS_OUT_OF_SPACE;

					// CALCULATE TOTAL SPACE ALLOCATED VIA SEEK
					unsigned int totalSize = (i+1) * SOFTWARE_DISK_BLOCK_SIZE;

					// IF TOTAL SPACE EXCEEDED OLD FILE SIZE, UPDATE FILE SIZE
					if(totalSize > fileSize)
						update_file_size((*file).recordNumber, totalSize);

					return;
				}

			// ALLOCATE
			allocate_data_block(&currentBlock, freeBlock);

			// CONTEXT SWITCH
			currentBlock = freeBlock;
		}
		else
		{
			// JUST CONTEXT SWITCH
			currentBlock = next;
		}
	}

	// UPDATE FILEINTERNALS
	if(bytepos > fileSize)
		update_file_size((*file).recordNumber, bytepos);

	(*file).currentBlock = currentBlock;

	(*file).filePos = bytepos;
}

// returns the current length of the file in bytes. Always sets 'fserror' global.
unsigned long file_length(File file)
{


	return (*file).fileSize;
}

// deletes the file named 'name', if it exists and if the file is closed. 
// Fails if the file is currently open. Returns 1 on success, 0 on failure. 
// Always sets 'fserror' global.   
int delete_file(char *name)
{
	#define numFatBlocks info.numFatBlocks
	#define numRecordBlocks info.numRecordBlocks
	#define firstFatBlock info.firstFatBlock
	#define firstRecordBlock info.firstRecordBlock

	FSInfo info = get_fs_info();

	Error = FS_NONE;

	unsigned int recordNumber = find_file(name);

	if (recordNumber == 0xFFFFFFFF)
	{
		//printf("Failed to delete %s - File Not Found\n", name);
		Error = FS_FILE_NOT_FOUND;
		return 0;
	}

	unsigned int recordsPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_RECORD_ENTRY;
	unsigned int blockIndex = recordNumber / recordsPerBlock;
	unsigned int recordIndex = recordNumber - (blockIndex * recordsPerBlock);
	unsigned int recordOffset = recordIndex * SIZE_OF_RECORD_ENTRY;

	unsigned int absBlockNumber = blockIndex + firstRecordBlock;

	// Get Record Block
	char* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	read_sd_block(blockData, absBlockNumber);

	// Check if File is Open
	unsigned char fileAttr;
	memcpy(&fileAttr, blockData + recordOffset, sizeof(char));

	// IF FILE IS OPEN
	if(isNthBitSet(fileAttr, 2))
	{
		//printf("Failed to delete %s - File Open\n", name);
		Error = FS_FILE_OPEN;
		free(blockData);
		return 0;
	}

	// Bit-Mask for number of records for this File
	unsigned int numRecords = fileAttr & 15;

	// Store Data Block index for clearing later
	unsigned int firstBlock;
	memcpy(&firstBlock, blockData + recordOffset + 1, sizeof(int));

	// Store File Size to index data deletion
	unsigned int fileSize;
	memcpy(&fileSize, blockData + recordOffset + 5, sizeof(int));
	unsigned int numBlocks = (int)ceil(fileSize + 0.001 / SOFTWARE_DISK_BLOCK_SIZE);

	// Zeroizing the records (deletion)
	char* zeroize = calloc(numRecords * SIZE_OF_RECORD_ENTRY, sizeof(char));
	memcpy(blockData + recordOffset, zeroize, sizeof(zeroize));

	// Writing changes to Record Block
	write_sd_block(blockData, absBlockNumber);

	// Calculate block holding FAT entry
	unsigned int entriesPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_FAT_ENTRY;
	blockIndex = firstBlock / entriesPerBlock;
	unsigned int entryIndex = firstBlock - (blockIndex * entriesPerBlock);
	unsigned int entryOffset = entryIndex * SIZE_OF_FAT_ENTRY;

	absBlockNumber = blockIndex + firstFatBlock;

	read_sd_block(blockData, absBlockNumber);

	unsigned int currentValue;
	memcpy(&currentValue, blockData + entryOffset, sizeof(int));

	for(int i = 0; i < numBlocks; i++)
	{
		if(currentValue != 0xFFFFFFFF)
		{
			// Zeroize value of current FAT entry
			unsigned int zero = 0x00000000;
			memcpy(blockData + entryOffset, &zero, sizeof(int));
			write_sd_block(blockData, absBlockNumber);

			// Calculate new offsets
			blockIndex = currentValue / entriesPerBlock;
			entryIndex = firstBlock - (blockIndex * entriesPerBlock);
			entryOffset = entryIndex * SIZE_OF_FAT_ENTRY;

			absBlockNumber = blockIndex + firstFatBlock;

			// Read FAT block with currentValue
			read_sd_block(blockData, absBlockNumber);

			// Get next value
			memcpy(&currentValue, blockData + entryOffset, sizeof(int));
		}
		else
		{
			// Zeroize value of current FAT entry
			unsigned int zero = 0x00000000;
			memcpy(blockData + entryOffset, &zero, sizeof(int));
			write_sd_block(blockData, absBlockNumber);

			//printf("Successfully deleted %s\n", name);
			free(blockData);
			return 1;
		}

		printf("Unknown Error 600\n");
	}

	#undef numFatBlocks
	#undef numRecordBlocks
	#undef firstFatBlock
	#undef firstRecordBlock
}

// determines if a file with 'name' exists and returns 1 if it exists, otherwise 0.
// Always sets 'fserror' global.
int file_exists(char *name)
{
	unsigned int exists = find_file(name);

	if(exists != 0xFFFFFFFF)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

// describe current filesystem error code by printing a descriptive message to standard
// error.
void fs_print_error(void)
{
	if(Error == FS_NONE)
		fprintf(stderr, "Operation Successful - No Error.\n");

	if(Error == FS_FILE_NOT_OPEN)
		fprintf(stderr, "Operation Failed - File Referenced not Open\n");

	if(Error == FS_FILE_OPEN)
		fprintf(stderr, "Operation Failed - File Already Open\n");

	if(Error == FS_FILE_NOT_FOUND)
		fprintf(stderr, "Operation Failed - File Not Found\n");

	if(Error == FS_FILE_ALREADY_EXISTS)
		fprintf(stderr, "Operation Failed - File Already Exists.\n");

	if(Error == FS_OUT_OF_SPACE)
		fprintf(stderr, "Operation Failed - Insufficient Space.\n");

	if(Error == FS_FILE_READ_ONLY)
		fprintf(stderr, "Operation Failed - File is Read-Only\n");
}


// =========================================================
// ======================MY FUNCTIONS=======================
// =========================================================

// Searches for File Record of 'name',
//  returns Record Index, 0xFFFFFFFF if not found
unsigned int find_file(char *name)
{
	#define numRecordBlocks info.numRecordBlocks
	#define firstRecordBlock info.firstRecordBlock

	FSInfo info = get_fs_info();

	char* blockData;

	// ========== RECORD CHECKING ==========
	// =====================================

	unsigned int nameLength = strlen(name);
	unsigned char recordsRequired = (char)ceil(nameLength/23.0);

	unsigned int recordsPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_RECORD_ENTRY;

	for(unsigned int blockIndex = 0; blockIndex < numRecordBlocks; blockIndex++)
	{
		unsigned int absBlockNumber = blockIndex + firstRecordBlock;

		blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
		read_sd_block(blockData, absBlockNumber);

		for(unsigned int recordIndex = 0; recordIndex < recordsPerBlock; recordIndex++)
		{
			unsigned int entryOffset = recordIndex * SIZE_OF_RECORD_ENTRY;

			unsigned char fileAttr;
			memcpy(&fileAttr, blockData + entryOffset, sizeof(char));

			// END OF RECORDS, FILE NOT FOUND
			if(fileAttr == 0)
			{
				return 0xFFFFFFFF;
			}

			// IF RECORD PRESENT
			if(isNthBitSet(fileAttr, 0))
			{
				// IF RECORD IS PARENT
				if(isNthBitSet(fileAttr, 1))
				{
					unsigned char numRecordsCurrent = fileAttr & 15;
					if(numRecordsCurrent == recordsRequired)
					{
						unsigned char fileName[numRecordsCurrent*23];

						for(unsigned int internalIndex = 0; internalIndex < numRecordsCurrent; internalIndex++)
						{
							memcpy((fileName + (internalIndex*23)), (blockData + entryOffset + (internalIndex*32) + 9), 23*sizeof(char));
						}

						// IF NAMES MATCH, FILE FOUND
						if(!strcmp(fileName, name))
						{
							return (recordIndex + (blockIndex * recordsPerBlock));
						}
					}
				}
			}
		}
	
		// FREE THE BUFFER!
		free(blockData);
	}

	#undef numRecordBlocks
	#undef firstRecordBlock
}

// Determines if File (given Record Number) is open
//  returns 1 for Open
//  returns 0 for Closed
unsigned int is_open(unsigned int recordNumber)
{
	#define firstRecordBlock info.firstRecordBlock
	FSInfo info = get_fs_info();

	// READ FILE RECORD BLOCK
	unsigned int recordsPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_RECORD_ENTRY;
	unsigned int blockIndex = recordNumber / recordsPerBlock;
	unsigned int recordIndex = recordNumber - (blockIndex * recordsPerBlock);
	unsigned int recordOffset = recordIndex * SIZE_OF_RECORD_ENTRY;

	char* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	read_sd_block(blockData, blockIndex + firstRecordBlock);

	// GET FILE ATTRIBUTES
	unsigned int fileAttr;
	memcpy(&fileAttr, blockData + recordOffset, sizeof(char));

	free(blockData);

	// IF -- FILE IS NOT OPEN
	if(!isNthBitSet(fileAttr, 2))
	{
		return 0;
	}
	else
	{
		return 1;
	}
	#undef firstRecordBlock
}

// Allocates a data block, updating parent's FAT value
unsigned int allocate_data_block(int* parentFatIndexPtr, int targetFatIndex)
{
	#define firstFatBlock info.firstFatBlock
	#define firstDataBlock info.firstDataBlock

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

	// ZEROIZE THE DATA BLOCK

		// read data block into blockData
		read_sd_block(blockData, (targetFatIndex + firstDataBlock));

		// create zeroizer
		char* zeroizer = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));

		// copy zeroizer
		memcpy(blockData, zeroizer, SOFTWARE_DISK_BLOCK_SIZE);
		free(zeroizer);

		// write data block
		write_sd_block(blockData, (targetFatIndex + firstDataBlock));


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
	#undef firstDataBlock
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

	#undef firstRecordBlock
}

// Finds and Returns the FAT Index of the first free Data Block
//  ~~ Must offset by +firstDataBlock to read/write block
//  ~~ Returns 0xFFFFFFFF if FS_OUT_OF_SPACE
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
	return 0xFFFFFFFF;

	#undef firstFatBlock
	#undef numFatBlocks
	#undef firstDataBlock
}

// Returns index of child of parentIndex
//  Returns 0xFFFFFFFF on terminating entry
unsigned int get_next_data_block(unsigned int parentIndex)
{
	FSInfo info = get_fs_info();

	#define firstFatBlock info.firstFatBlock
	#define numFatBlocks info.numFatBlocks

	unsigned int entriesPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_FAT_ENTRY;
	unsigned int blockIndex = parentIndex / entriesPerBlock;
	unsigned int entryIndex = parentIndex - (blockIndex * entriesPerBlock);
	unsigned int entryOffset = entryIndex * SIZE_OF_FAT_ENTRY;

	// READ PARENTS FAT BLOCK
	char* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	read_sd_block(blockData, blockIndex + firstFatBlock);

	// GET PARENTS FAT VALUE
	unsigned int childIndex;
	memcpy(&childIndex, blockData + entryOffset, sizeof(int));

	free(blockData);
	return childIndex;

	#undef firstFatBlock
	#undef numFatBlocks
}

// Finds and Returns the Record Number of the first free contiguous Record entries of length
//  SETS FS_OUT_OF_SPACE IF ERROR
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

// Creates a new file record
//  Returns the index number of the File Record created
unsigned int write_record_entry(char* name, unsigned int dataBlock)
{
	struct FSInfo info = get_fs_info();
	#define firstRecordBlock info.firstRecordBlock

	// Calculate number of records needed for File Name
	unsigned int length = strlen(name);
	unsigned int recordsRequired = (int)ceil(length/23.0);

	// Get record we're going to write into
	unsigned int parentRecordIndex = get_free_record(recordsRequired);
	if(Error == FS_OUT_OF_SPACE)
		return 0;

	// Indexing
	unsigned int recordsPerBlock = SOFTWARE_DISK_BLOCK_SIZE / SIZE_OF_RECORD_ENTRY;
	unsigned int parentRecordBlockNumber = parentRecordIndex / recordsPerBlock;
	unsigned int parentInternalIndex = parentRecordIndex - (parentRecordBlockNumber * recordsPerBlock);

	// Read Block of Parent Record
	char* blockData = calloc(SOFTWARE_DISK_BLOCK_SIZE, sizeof(char));
	read_sd_block(blockData, (parentRecordBlockNumber + firstRecordBlock));

	// Write "First Data Block" field to buffer (Only done in Parent record)
	memcpy((blockData + (parentInternalIndex * SIZE_OF_RECORD_ENTRY) + 1), &dataBlock, sizeof(int));

	for(unsigned int clusterIndex = 0; clusterIndex < recordsRequired; clusterIndex++)
	{
		// ===== CONSTRUCT FIRST BYTE =====
		// ================================
		unsigned char fileAttr = 0x00;

		fileAttr |= 128; // Set Present Flag

		// IF Parent Block
		if(clusterIndex == 0)
		{
			fileAttr |= 64; // Set Parent Flag
			fileAttr |= 32; // Set Open Flag (this function only called by Create_File)
		}

		fileAttr |= (recordsRequired - clusterIndex); // Set Cluster Index

		// Write first Byte
		memcpy((blockData + (parentInternalIndex * SIZE_OF_RECORD_ENTRY) + (clusterIndex * SIZE_OF_RECORD_ENTRY)), &fileAttr, sizeof(char));

		// Write Name
		if(length <= 23)
		{
			// Write 'length' bytes to name field
			memcpy((blockData + (parentInternalIndex * SIZE_OF_RECORD_ENTRY) + (clusterIndex * SIZE_OF_RECORD_ENTRY) + 9), name + (clusterIndex * 23), (length * sizeof(char)));

		}
		else
		{
			// Write 23 bytes to name field
			memcpy((blockData + (parentInternalIndex * SIZE_OF_RECORD_ENTRY) + (clusterIndex * SIZE_OF_RECORD_ENTRY) + 9), name + (clusterIndex * 23), (23 * sizeof(char)));

			// Then decrease length by 23
			length -= 23;
		}

	}

	// Write to Disk
	write_sd_block(blockData, (parentRecordBlockNumber + firstRecordBlock));

	free(blockData);
	return parentRecordIndex;
	#undef firstRecordBlock

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
int isNthBitSet (unsigned char c, int n)
{
    static unsigned char mask[] = {128, 64, 32, 16, 8, 4, 2, 1};
    return ((c & mask[n]) != 0);
}