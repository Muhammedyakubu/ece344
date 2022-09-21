#include <errno.h>
#include <string.h>
#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
/* make sure to use syserror() when a system call fails. see common.h */

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

// TODO: modify this function to apply permissions from src to dst
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

	// close src
	close(fd_src);

	// give all users access to the new file
	chmod(dst, 0777); // use 7777? or call stat and use srcdir's?
	// close the new file
	close(fd_dst);

	return 1;
}

int
copy_dir(char *dstdir, char *srcdir) {
	// opendir srcdir 
	DIR *src;
	if((src = opendir(srcdir)) == NULL) {
		syserror(opendir, srcdir);
	}
	// get src stat
	struct stat src_st = {0};
	if (stat(srcdir, &src_st) == -1) {
		syserror(stat, srcdir);
	}

	// mkdir dstdir with the same mode as the srcdir
	DIR *dst;
	if(mkdir(dstdir, src_st.st_mode) < 0) {
		syserror(mkdir, dstdir);
	}
	// if no error, then open the newly created dstdir
	if((dst = opendir(dstdir)) == NULL) {
		syserror(opendir, dstdir);
	}


	// iteratively readdir srcdir, and duplicate the entries in dstdir
	errno = 0;
	struct dirent *entry = NULL;
	struct stat e_stat = {0};

	while((entry = readdir(src))) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
			continue;
		}

		
		// construct the linux path for each entry in the srcdir
		char entry_dstdir[256]; 
		char entry_srcdir[256];

		strcpy(entry_dstdir, dstdir);	
		strcpy(entry_srcdir, srcdir);	

		strcat(entry_dstdir, "/");
		strcat(entry_srcdir, "/");

		strcat(entry_dstdir, entry->d_name);
		strcat(entry_srcdir, entry->d_name);

		// is entry a dir or file? (stat)
		// first get stat
		if (stat(entry_srcdir, &e_stat) == -1) {
			syserror(stat, entry_srcdir);
		}

		printf("Processing entry: %s in dir: %s with ISREG: %d & ISDIR: %d\n", entry->d_name, srcdir, S_ISREG(e_stat.st_mode), S_ISDIR(e_stat.st_mode));

		if(S_ISREG(e_stat.st_mode)) {
			// if it's a regular file, call copy_file
			copy_file(entry_dstdir, entry_srcdir);
			
		} else if (S_ISDIR(e_stat.st_mode)) {
			// recursively call copy_dir
			copy_dir(entry_dstdir, entry_srcdir);
		} else {
			printf("Directory entry at %s was neither a file or directory", entry_srcdir);
		}
	}

	// if errno was set then there was an error
	if(errno) {
		syserror(readdir, srcdir);
	}

	// closedir both dir 
	closedir(src);
	closedir(dst);

	return 0;
}

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		usage();
		return 0;
	}

	char *srcdir = argv[1];
	char *dstdir = argv[2];

	printf("src is:%s and dst is:%s\n", srcdir, dstdir);

	//	check if input is file or directory
	struct stat src_st = {0};
	if (stat(srcdir, &src_st) == -1) {
		syserror(stat, srcdir);
	}

	
	if(S_ISREG(src_st.st_mode)) {
		// if it's a regular file, call copy_file
		copy_file(dstdir, srcdir);
		
	} else if (S_ISDIR(src_st.st_mode)) {
		// recursively call copy_dir
		copy_dir(dstdir, srcdir);
	} else {
		printf("Input Directory entry at %s was neither a file or directory", srcdir);
	}

	return 0;
}
