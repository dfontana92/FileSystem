#include <stdio.h>
#include "filesystem.h"

// Testing Only
#include "softwaredisk.h"

int main()
{
	int a = 500;
	a = get_free_data_block();
	printf("First free data block: %i\n", a);

	int b = 500;
	b = get_free_data_block();
	printf("Same free data block: %i\n", b);

	unsigned int val = 14;
	write_fat_entry(0, val);

	int c = 500;
	c = get_free_data_block();
	printf("Next free data block: %i\n", c);
}