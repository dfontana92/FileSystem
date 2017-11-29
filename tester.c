#include <stdio.h>
#include "filesystem.h"


int main()
{
	File f = create_file("Testing", READ_WRITE);
	File f2 = create_file("TestingTestingTestingTesting", READ_ONLY);
	File f3 = create_file("whatup", READ_ONLY);

	printf("\nCreated Files\n");

	File f4 = open_file("Testingg", READ_ONLY);

	open_file("Testing", READ_ONLY);

	close_file(f3);
	open_file("whatup", READ_WRITE);

}