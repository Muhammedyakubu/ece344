#include "common.h"
#include <ctype.h>

int fact(int n){
	if(n==0 || n==1)
		return n;
	return n * fact(n-1);
}

int main(int argc, char *argv[])
{
	if(argc < 2) {
		printf("Huh?\n");
		return 0;
	}

	// check that input is a valid int
	char *s = argv[1];

	// traverse the input string until we find a non-digit
	for(int i = 0; s[i] != '\0'; i++){
		if (!isdigit(s[i])) {
			printf("Huh?\n");
			return 0;
		}
	}
	
	int num = atoi(argv[1]);

	if (num > 12) {
		printf("Overflow\n");
		return 0;
	}

	printf("%d\n", fact(num));
	return 0;
}
