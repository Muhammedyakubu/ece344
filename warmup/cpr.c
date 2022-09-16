#include <errno.h>
#include <string.h>
#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
/* make sure to use syserror() when a system call fails. see common.h */

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

int
copy_file(char *dst, char *src) {
	// open the src file
	int fd_src;
	if ((fd_src = open(src, O_RDONLY)) < 0) {
		syserror(open, src);
	}

	// create & open the dst file
	int fd_dst;
	if ((fd_dst = open(dst, O_RDWR | O_CREAT)) < 0) {
		syserror(open, dst);
	}
	
	// in a loop, read from the src file to the dst file
	int bytes_read;
	int BUF_SIZE = 4096;
	char buf[BUF_SIZE];
	while((bytes_read = read(fd_src, buf, BUF_SIZE)) > 0) {
		if(write(fd_dst, buf, bytes_read) < 0) {
			syserror(write, dst);
		}
	}

	// error handling
	if (bytes_read < 0) {
		syserror(read, src);
	}

	// close the both files
	close(fd_src);

	// give all users access to the new file
	chmod(dst, 777);
	close(fd_dst);

	return 1;
}

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		usage();
		return 0;
	}

	char *src = argv[1];
	char *dst = argv[2];

	printf("src is:%s and dst is:%s\n", src, dst);

	// TODO: check if input is file or directory

	copy_file(dst, src);

	return 0;
}
