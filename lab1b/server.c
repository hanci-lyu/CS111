#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <mcrypt.h>
#include <fcntl.h>


struct arguments {
  uint16_t  port_no;
  int       eflag;
} args;

void parse(int argc, char *argv[]) 
{
  struct option longOptions[] = 
  {
    {"port", required_argument, NULL, 'p'},
    {"encrypt", no_argument, NULL, 'e'},
    {NULL, 0, NULL, 0},
  };

  int opt, pflag = 0;

  while ( (opt = getopt_long(argc, argv, "p:e", longOptions, NULL)) != -1 )
  {
    switch (opt) 
    {
    case 'p':
      args.port_no = atoi(optarg);
      pflag = 1;
      break;
    case 'e':
      args.eflag = 1;
      break;
    default:
      exit(-1);
      break;
    }
  }

  if (pflag == 0) {
    fprintf(stderr, "No port number is provided!\n");
    exit(-1);
  }

}

pid_t shell_pid;

void sigpipe_handler(int signum)
{
  // close network
  close(0);
  close(1);
  close(2);

  // kill shell
  if (shell_pid > 0) 
    kill(shell_pid, SIGINT);

  exit(2);
}

MCRYPT td;
void *thread_entry(void *arg)
{
  int *p = (int *)arg;
  char buf;
  ssize_t insize;

  while ((insize = read(p[0], &buf, 1)) > 0) {
    if (args.eflag)
      mcrypt_generic(td, &buf, 1);
    write(1, &buf, 1);
  }

  // close network
  close(0);
  close(1);
  close(2);

  // kill shell
  if (shell_pid > 0)
    kill(shell_pid, SIGINT);

  exit(2);
}

int main (int argc, char *argv[])
{
  // parse arguments
  parse(argc, argv);

  // --encrypt option
  char key[16];
  char *IV;
  if (args.eflag == 1) {
    td = mcrypt_module_open("twofish", NULL, "cfb", NULL);
    if (td==MCRYPT_FAILED)
      exit(-1);

    // read in the key
    int key_fd = open("my.key", O_RDONLY);
    read(key_fd, key, 16);
    close(key_fd);

    // init IV
    int i;
    IV = malloc(mcrypt_enc_get_iv_size(td));
    srand(1);
    for (i = 0; i< mcrypt_enc_get_iv_size( td); i++)
      IV[i] = rand();

    mcrypt_generic_init(td, key, 16, IV);
  }

  // create socket
  int sockfd_acc, sockfd;
  struct sockaddr_in server_addr, client_addr;
  sockfd_acc = socket(AF_INET , SOCK_STREAM , 0);
  if (sockfd_acc == -1) {
    perror("Error in socket()");
    exit(-1);
  }
  // int reuse = 1;
  // if (setsockopt(sockfd_acc, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
  //     perror("setsockopt(SO_REUSEADDR) failed");

  // set up server addr
  memset((char *) &server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(args.port_no);
  
  // bind to port_no
  if (bind(sockfd_acc, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
    perror("Error in bind()");
    exit(-1);
  }

  // listen for connection
  if (listen(sockfd_acc, 5) == -1) {
    perror("Error in listen()");
    exit(-1);
  }

  // accept a new connection from client
  socklen_t clen = sizeof(client_addr);
  if ((sockfd = accept(sockfd_acc, (struct sockaddr *) &client_addr, &clen)) == -1) {
    perror("Error in accept()");
    exit(-1);
  }
  close(sockfd_acc);

  // redirect stdin/stdout/stderr
  dup2(sockfd, 0);
  dup2(sockfd, 1);
  dup2(sockfd, 2);
  close(sockfd);

  // create pipes betweem server and shell
  int pipe_to_shell[2], pipe_from_shell[2];
  if (pipe(pipe_from_shell) == -1 || pipe(pipe_to_shell) == -1) {
    perror("Error in pipe()");
    exit(-1);
  }

  // do fork
  pthread_t thread;
  if ((shell_pid = fork()) == 0) { // shell process
    
    // set up pipe from server to shell
    dup2(pipe_to_shell[0], 0);
    close(pipe_to_shell[0]);
    close(pipe_to_shell[1]);

    // set up pipe from shell to server
    dup2(pipe_from_shell[1], 1);
    dup2(pipe_from_shell[1], 2);
    close(pipe_from_shell[1]);
    close(pipe_from_shell[0]);

    execlp("/bin/bash", "/bin/bash", NULL);
  }
  else if (shell_pid > 0) { // parent process
    close(pipe_to_shell[0]);
    close(pipe_from_shell[1]);

    // set handlers
    signal(SIGPIPE, sigpipe_handler);

    // create a thread to read from shell and write to socket
    if (pthread_create(&thread, NULL, thread_entry, (void *) pipe_from_shell) != 0) {
      perror("Error in pthread_create");
      exit(-1);
    }

  }
  else {
    perror("Error in fork()");
    exit(-1);
  }

  // read from socket and write to pipe
  char buf;
  ssize_t insize = 0;
  while ((insize = read(0, &buf, 1)) > 0) {
    if (args.eflag)
      mdecrypt_generic (td, &buf, 1);
    write(pipe_to_shell[1], &buf, 1);
  }

  // EOF
  close(0);
  close(1);
  close(2);
  if (shell_pid > 0) 
    kill(shell_pid, SIGINT);
  exit(1);
}
