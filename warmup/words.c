#include "common.h"

int
main(int argc, char *argv[])
{
	for(int i = 1; i < argc; i++){
		printf(argv[i]);
		printf("\n");
	}
	return 0;
}
