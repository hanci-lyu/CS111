#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>

struct termios old_attr;

void restore_terminal()
{
  tcsetattr(0, TCSAFLUSH, &old_attr);
}

void set_terminal()
{
  struct termios new_attr;

  // save the old attributes
  tcgetattr (0, &old_attr);
  atexit (restore_terminal);

  // set to noncanonical mode, no echo
  tcgetattr (0, &new_attr);
  new_attr.c_lflag &= ~(ICANON|ECHO);
  new_attr.c_cc[VMIN] = 1;
  new_attr.c_cc[VTIME] = 0;
  tcsetattr (0, TCSAFLUSH, &new_attr);
}

int parse(int argc, char *argv[]) 
{
  struct option longOptions[] = 
  {
    {"shell", no_argument, NULL, 's'},
    {NULL, 0, NULL, 0},
  };

  int opt, sflag=0;

  while ( (opt = getopt_long(argc, argv, "s", longOptions, NULL)) != -1 )
  {
    switch (opt) 
    {
    case 's':
      sflag = 1;
      break;

    default:
      exit(4);
      break;

    }
  }

  return sflag;
}

pid_t child_pid;

void sigint_handler(int signum)
{
  if (child_pid > 0) kill(child_pid, SIGINT);
}

void sigpipe_handler(int signum)
{
  exit(1);
}

void *thread_entry(void *arg)
{
  int *p = (int *)arg;
  char buf;
  ssize_t insize;

  while ((insize = read(p[0], &buf, 1)) > 0)
  // read will return 0 when it sees EOF from the pipe 
  {
    write(1, &buf, 1);
  }

  if (insize < 0) perror("Read from shell");

  // insize equals 0 (EOF)
  exit(1);
}

void print_shell_status()
{
  int status;
  if (child_pid > 0 && waitpid(child_pid, &status, 0) > 0)
  {
    if (WIFEXITED(status)) {
      fprintf(stdout, "Shell exited with return code %d\n",WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status)) {
      fprintf(stdout, "Shell terminated by signal %d\n",WTERMSIG(status));
    }
  }
}

int main (int argc, char *argv[])
{
  // parse arguments
  int sflag = parse(argc, argv);

  // buffer of one byte
  char buf;

  // canonical mode
  set_terminal();

  // --shell option
  int p1[2], p2[2];
  pthread_t thread;
  if (sflag) 
  {
    // create pipes
    if (pipe(p1) == -1 || pipe(p2) == -1) 
    {
      perror("Create pipe");
      exit(4);
    }

    // do fork
    if ((child_pid = fork()) == 0) 
    // shell process
    {
      // pipe from terminal to shell  
      dup2(p1[0], 0);
      close(p1[0]);
      close(p1[1]);

      // pipe from shell to terminal
      dup2(p2[1], 1);
      dup2(p2[1], 2);
      close(p2[0]);
      close(p2[1]);

      execlp("/bin/bash", "/bin/bash", NULL);
    }
    else
    // parent process
    {
      close(p1[0]);
      close(p2[1]);

      // set handlers
      signal(SIGPIPE, sigpipe_handler);
      signal(SIGINT, sigint_handler);

      // set callback
      atexit(print_shell_status);

      // create a thread
      pthread_create(&thread, NULL, thread_entry, (void*)p2);

    }
  }

  ssize_t insize = 0;
  while ((insize = read(0, &buf, 1)) > 0)
  {
    if (buf == '\004')
    {
      if (sflag)
      {
        pthread_cancel(thread);
        pthread_join(thread, NULL);
        kill(child_pid, SIGHUP);
        close(p1[1]);
      }
      exit(0);
    }
    else if (buf == '\003')
    {
      if (sflag && child_pid > 0) kill(child_pid, SIGINT);
    }
    else if (buf == '\r' || buf == '\n')
    {
      write(1, "\r\n", 2);
      if (sflag) write(p1[1], "\n", 1);
    }
    else
    {
      write(1, &buf, 1);
      if (sflag) write(p1[1], &buf, 1);
    }
  }
  if (insize < 0)
    perror(argv[0]);

  return 0;
}
