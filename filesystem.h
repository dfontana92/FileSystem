#define SIZE_OF_FAT_ENTRY     (1 * sizeof(int))
#define SIZE_OF_RECORD_ENTRY  (32 * sizeof(char))

// access mode for open_file() and create_file() 
typedef enum {
  READ_ONLY, READ_WRITE
} FileMode;

// main private file type
typedef struct FileInternals
{
    unsigned int recordNumber;
    unsigned int fileSize;
    unsigned int filePos;
    unsigned int startingBlock;
    unsigned int currentBlock;
    FileMode mode;
} FileInternals;

// file type used by user code
typedef FileInternals* File;

// FS Information Struct
//  numFatBlocks  (bytes 0-3)
//  numRecordBlocks  (bytes 4-7)
//  numDataBlocks (bytes 8-11)
//  firstFatBlock (bytes 12-15)
//  firstRecordBlock (bytes 16-19)
//  firstDataBlock  (bytes 20-23)
//  lastUsedBlock (bytes 24-27) - starts as 0, no need to write
typedef struct FSInfo {
    unsigned int numFatBlocks;
    unsigned int numRecordBlocks;
    unsigned int numDataBlocks;
    unsigned int firstFatBlock;
    unsigned int firstRecordBlock;
    unsigned int firstDataBlock;
    unsigned int lastUsedBlock;
} FSInfo;

// error codes set in global 'fserror' by filesystem functions
typedef enum  {
  FS_NONE, 
  FS_OUT_OF_SPACE,        // the operation caused the software disk to fill up
  FS_FILE_NOT_OPEN,  	  // attempted read/write/close/etc. on file that isn’t open
  FS_FILE_OPEN,      	  // file is already open. Concurrent opens are not
                          // supported and neither is deleting a file that is open.
  FS_FILE_NOT_FOUND, 	  // attempted open or delete of file that doesn’t exist
  FS_FILE_READ_ONLY, 	  // attempted write to file opened for READ_ONLY
  FS_FILE_ALREADY_EXISTS  // attempted creation of file with existing name
} FSError;

// function prototypes for filesystem API

// open existing file with pathname 'name' and access mode 'mode'.  Current file
// position is set at byte 0.  Returns NULL on error. Always sets 'fserror' global.
File open_file(char *name, FileMode mode);

// create and open new file with pathname 'name' and access mode 'mode'.  Current file
// position is set at byte 0.  Returns NULL on error. Always sets 'fserror' global.
File create_file(char *name, FileMode mode);

// close 'file'.  Always sets 'fserror' global.
void close_file(File file);

// read at most 'numbytes' of data from 'file' into 'buf', starting at the 
// current file position.  Returns the number of bytes read. If end of file is reached,
// then a return value less than 'numbytes' signals this condition. Always sets
// 'fserror' global.
unsigned long read_file(File file, void *buf, unsigned long numbytes);

// write 'numbytes' of data from 'buf' into 'file' at the current file position. 
// Returns the number of bytes written. On an out of space error, the return value may be
// less than 'numbytes'.  Always sets 'fserror' global.
unsigned long write_file(File file, void *buf, unsigned long numbytes);

// sets current position in file to 'bytepos', always relative to the beginning of file.
// Seeks past the current end of file should extend the file. Always sets 'fserror'
// global.
void seek_file(File file, unsigned long bytepos);

// returns the current length of the file in bytes. Always sets 'fserror' global.
unsigned long file_length(File file);

// deletes the file named 'name', if it exists and if the file is closed. 
// Fails if the file is currently open. Returns 1 on success, 0 on failure. 
// Always sets 'fserror' global.   
int delete_file(char *name); 

// determines if a file with 'name' exists and returns 1 if it exists, otherwise 0.
// Always sets 'fserror' global.
int file_exists(char *name);

// describe current filesystem error code by printing a descriptive message to standard
// error.
void fs_print_error(void);

// filesystem error code set (set by each filesystem function)
extern FSError fserror;

// ========== MY FUNCTIONS ==========
// ==================================

struct FSInfo get_fs_info();

unsigned int find_file(char *name);

unsigned int is_open(unsigned int recordNumber);

// Returns index of first free entry in the FAT
//  Returns 0xFFFFFFFF on OUT_OF_SPACE error
unsigned int get_free_data_block();

// Returns index of first record at start of 'length' contiguous records
//  Returns 0xFFFFFFFF on OUT_OF_SPACE error
unsigned int get_free_record(unsigned int length);

// Returns index of child of parentIndex
//  Returns 0xFFFFFFFF on terminating entry
unsigned int get_next_data_block(unsigned int parentIndex);

unsigned int update_file_size(unsigned int recordNumber, unsigned int size);

unsigned int write_fat_entry(unsigned int entryNumber, unsigned int entryValue);

unsigned int write_record_entry(char* name, unsigned int dataBlock);

unsigned int allocate_data_block(int* parentFatIndexPtr, int targetFatIndex);

// Endian-ness sucks
void iEndianSwap(int* num);

// Bitwise Comparison Function
int isNthBitSet(unsigned char c, int n);