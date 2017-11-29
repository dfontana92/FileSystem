#include <stdio.h>
#include "filesystem.h"


int main()
{
	File f = create_file("Testing", READ_WRITE);
	File f2 = create_file("TestingTestingTestingTesting", READ_ONLY);
	File f3 = create_file("whatup", READ_ONLY);

	printf("Created File\n");
}