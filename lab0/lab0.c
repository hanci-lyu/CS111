#include <getopt.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

struct arguments
{
	int iflag;
	int oflag;
	int sflag;
	int cflag;
	char *infile;
	char *outfile;
} args;

void parse(int argc, char *argv[]) 
{
	struct option longOptions[] = 
	{
		{"input", required_argument, NULL, 'i'},
		{"output", required_argument, NULL, 'o'},
		{"segfault", no_argument, NULL, 's'},
		{"catch", no_argument, NULL, 'c'},
		{NULL, 0, NULL, 0},
	};

	int opt;

	while ( (opt = getopt_long(argc, argv, "i:o:sc", longOptions, NULL)) != -1 )
	{
		switch (opt) 
		{
		case 'i':
			args.iflag = 1;
			args.infile = optarg;
			break;

		case 'o':
			args.oflag = 1;
			args.outfile = optarg;
			break;

		case 's':
			args.sflag = 1;
			break;

		case 'c':
			args.cflag = 1;
			break;

		default:
			exit(4);
			break;

		}
	}
}

void handler(int signum)
{
	fprintf(stderr, "a segmentation fault has been caught\n");
	exit(3);
}

int main(int argc, char *argv[]) 
{
	const int BUF_SIZE = 1024;
	char buf[BUF_SIZE];
	ssize_t insize = 0;

	parse(argc, argv);

// register a handler
	if (args.cflag) 
		signal(SIGSEGV, handler);

// segmentation fault
	if (args.sflag)
	{
		char *badpointer = NULL;
		*badpointer = 0;
	}

// redirect input
	if (args.iflag)
	{
		int fd = open(args.infile, O_RDONLY);
		if (fd >= 0)
		{
			close(0);
			dup(fd);
			close(fd);
		} else 
		{
			fprintf(stderr, "%s: %s -- \'%s\'\n", argv[0], strerror(errno), args.infile);
			exit(1);
		}
	}

// redirect output
	if (args.oflag)
	{
		// int fd = open(args.outfile, O_WRONLY | O_CREAT | O_EXCL, 0666);
		int fd = creat(args.outfile, 0666);
		if (fd >= 0)
		{
			close(1);
			dup(fd);
			close(fd);
		} else 
		{
			fprintf(stderr, "%s: %s -- \'%s\'\n", argv[0], strerror(errno), args.outfile);
			exit(2);
		}
	}

	while ( (insize = read(0, buf, BUF_SIZE)) > 0 )
		write(1, buf, insize);

	return 0;

}