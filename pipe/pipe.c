#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static void check_error(int ret, const char *msg) {
  if (ret == -1) {
	int err = errno;
	perror(msg);
	exit(err);
  }
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		return EINVAL;
	}

	// save root's STDIN and STDOUT
	int root_stdin = dup(STDIN_FILENO);
	check_error(root_stdin, "dup");
	int root_stdout = dup(STDOUT_FILENO);
	check_error(root_stdout, "dup");

	fprintf(stderr, "argc: %d\n", argc);
	fprintf(stderr, "root's stdin: %d, stdout: %d\n", root_stdin, root_stdout);
	fflush(stderr);

	// loop to create new procs and set their stdin to the previous proc's stdout
	int pipefd[2];
	for (int i = 1; i < argc; ++i) {
		check_error(pipe(pipefd), "pipe");
		fprintf(stderr, "pipefd: %d, %d\n", pipefd[0], pipefd[1]);

		int child_pid = fork();
		check_error(child_pid, "fork");

		if (child_pid == 0) {
			// child (proc)
			// set stdin to previous proc's stdout

			// if first proc, set stdin to parent's stdin
			// else set stdin to previous proc's stdout
			int stdin_ = (i == 1) ? root_stdin : pipefd[0];
			// error checking
			check_error(dup2(stdin_, STDIN_FILENO), "dup2");

			// if last proc, set stdout to parent's stdout
			// else set stdout to next proc's stdin
			int stdout_ = (i == argc - 1) ? root_stdout : pipefd[1];
			// error checking
			check_error(dup2(stdout_, STDOUT_FILENO), "dup2");

			// should close other dangling fds in the child
			check_error(close(root_stdin), "close");
			check_error(close(root_stdout), "close");
			check_error(close(pipefd[0]), "close");
			check_error(close(pipefd[1]), "close");

			// exec proc
			fprintf(stderr, "exec %s\n", argv[i]);
			fflush(stderr);

			check_error(execlp(argv[i], argv[i], NULL), "execlp");
		}

		// parent (proc)
		
		check_error(close(pipefd[0]), "close");
		check_error(close(pipefd[1]), "close");

	}

	// wait for all children to finish
	int status;
	for (int i = 1; i < argc; ++i) {
		check_error(wait(&status), "wait");
	}

  	
	return EINVAL;
}
