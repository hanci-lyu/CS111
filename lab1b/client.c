#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <getopt.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <mcrypt.h>
#include <fcntl.h>

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

struct arguments {
  uint16_t  port_no;
  int       lflag;
  int       eflag;
  char      *log_name;
} args;

void parse(int argc, char *argv[]) 
{
  struct option longOptions[] = 
  {
    {"port", required_argument, NULL, 'p'},
    {"log", required_argument, NULL, 'l'},
    {"encrypt", no_argument, NULL, 'e'},
    {NULL, 0, NULL, 0},
  };

  int opt, pflag = 0;

  while ( (opt = getopt_long(argc, argv, "p:l:e", longOptions, NULL)) != -1 )
  {
    switch (opt) 
    {
    case 'p':
      args.port_no = atoi(optarg);
      pflag = 1;
      break;
    case 'l':
      args.log_name = optarg;
      args.lflag = 1;
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

void sigpipe_handler(int signum)
{
  exit(1);
}

MCRYPT td;
int log_fd;
void *thread_entry(void *arg)
{
  int *ptr_fd = (int *) arg;
  char buf;
  ssize_t insize;

  while ((insize = read(*ptr_fd, &buf, 1)) > 0) {
    if (args.lflag) {
      write(log_fd, "RECEIVED 1 bytes: ", 18);
      write(log_fd, &buf, 1);
      write(log_fd, "\n", 1);
    }
    if (args.eflag)
      mdecrypt_generic (td, &buf, 1);
    write(1, &buf, 1);
  }

  // read error / EOF
  close(*ptr_fd);
  exit(1);
}

int main (int argc, char *argv[])
{
  // parse arguments
  parse(argc, argv);

  // canonical mode
  set_terminal();

  // encrypt option
  char key[16];
  char *IV;
  if (args.eflag) {
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

  // --log option
  if (args.lflag) {
    log_fd = creat(args.log_name, 0666);
    if (log_fd < 0) {
      perror("Error in creat()");
      exit(-1);
    }
  }

  // create socket
  int sockfd;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    perror("Error in socket()");
    exit(-1);
  }

  // get server
  struct hostent *server;
  server = gethostbyname("localhost");
  if (server == NULL) {
    perror("Error in gethostbyname()");
    exit(-1);
  }

  // set up server addr
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(args.port_no);
  memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

  // connect to server
  if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
    perror("Error in connect()");
    exit(-1);
  }

  // create a thread to read from socket and write to terminal
  pthread_t thread;
  if (pthread_create(&thread, NULL, thread_entry, (void*) &sockfd) != 0) {
    perror("Error in pthread_create()");
    exit(-1);
  }

  // read from terminal and write to socket
  char buf;
  ssize_t insize = 0;
  while ((insize = read(0, &buf, 1)) > 0) {
    if (buf == '\004')
    {
      pthread_cancel(thread);
      pthread_join(thread, NULL);
      close(sockfd);
      exit(0);
    }
    else if (buf == '\r' || buf == '\n') {
      buf = '\n';
      write(1, "\r\n", 2);
      if (args.eflag)
        mcrypt_generic(td, &buf, 1);
      if (args.lflag) {
        write(log_fd, "SENT 1 bytes: ", 14);
        write(log_fd, &buf, 1);
        write(log_fd, "\n", 1);
      }
      write(sockfd, &buf, 1);
    }
    else {
      write(1, &buf, 1);
      if (args.eflag)
        mcrypt_generic(td, &buf, 1);
      if (args.lflag) {
        write(log_fd, "SENT 1 bytes: ", 14);
        write(log_fd, &buf, 1);
        write(log_fd, "\n", 1);
      }
      write(sockfd, &buf, 1);
    }
  }
  if (insize < 0) {
    perror("Error in read()");
    exit(-1);
  }

  return 0;
}
