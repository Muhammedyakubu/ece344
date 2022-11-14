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

	int num_pipes = argc - 2;
	int pipes[10][2] = {0};	// at least 10 pipes

	// stdin -> argv[1] -> pipes[0][1] -> pipes[0][0] -> argv[2] -> pipes[1][1] -> pipes[1][0] -> argv[3] -> ... -> pipes[num_pipes-1][1] -> pipes[num_pipes-1][0] -> argv[argc-1] -> stdout

	// create pipes
	for (int i = 0; i < num_pipes; i++) {
		check_error(pipe(pipes[i]), "pipe");
	}

	// loop to create new procs and set their stdin to the previous proc's stdout
	for (int i = 1; i < argc; ++i) {

		int child_pid = fork();
		check_error(child_pid, "fork");

		if (child_pid == 0) {
			// child (proc)
			// set stdin to previous proc's stdout

			// if first proc, set stdin to parent's stdin
			// else set stdin to previous proc's stdout
			int stdin_ = (i == 1) ? root_stdin : pipes[i-2][0];
			// error checking
			check_error(dup2(stdin_, STDIN_FILENO), "dup2");
			// fprintf(stderr, "%d, %d, %d\n", stdin_, STDIN_FILENO, root_stdin);

			// if last proc, set stdout to parent's stdout
			// else set stdout to next proc's stdin
			int stdout_ = (i == argc - 1) ? root_stdout : pipes[i-1][1];
			// error checking
			check_error(dup2(stdout_, STDOUT_FILENO), "dup2");

			// should close other dangling fds in the child
			for (int j = 0; j < num_pipes; j++) {
				if (pipes[j][0] != stdin_)
					check_error(close(pipes[j][0]), "close");
				if (pipes[j][1] != stdout_)
					check_error(close(pipes[j][1]), "close");
			}

			// exec proc
			// fprintf(stderr, "exec %s\n", argv[i]);
			// fflush(stderr);

			check_error(execlp(argv[i], argv[i], NULL), "execlp");
		}

		// parent (proc)

	}

	// wait for all children to finish
	int status;
	for (int i = 1; i < argc; ++i) {
		check_error(wait(&status), "wait");
		assert(WIFEXITED(status));
		if (WEXITSTATUS(status) != 0) {
			exit(WEXITSTATUS(status));
		}
	}

  	
	return EINVAL;
}
