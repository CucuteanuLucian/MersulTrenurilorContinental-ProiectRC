#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
extern int errno;

#define error_handler(x) \
    {                    \
        perror(x);       \
        exit(0);         \
    }

void semnal(int signum)
{
  if (signum == SIGUSR1)
  {
    printf("[client]La revedere!...\n");
    exit(0);
  }
}

int port;

int main(int argc, char *argv[])
{
  int sd;
  struct sockaddr_in server;
  char msg[100];

  if (argc != 3)
  {
    printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
    return -1;
  }

  port = atoi(argv[2]);

  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    error_handler("Eroare la socket().\n");
  }

  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons(port);

  if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    error_handler("[client]Eroare la connect().\n");
  }
  pid_t pid = fork();
  if (pid != 0)
  {
    printf("[client]Te rog sa introduci o comada: ");
    while (1)
    {
      signal(SIGUSR1, semnal);
      bzero(msg, 100);
      fflush(stdout);
      fgets(msg, sizeof(msg), stdin);
      printf("\n%s\n", msg);

      size_t marimeMsg = strlen(msg);
      if (write(sd, &marimeMsg, sizeof(size_t)) < 0)
      {
        error_handler("[client]Eroare la dimensiune write() spre server.\n");
      }
      if (write(sd, msg, marimeMsg) < 0)
      {
        error_handler("[client]Eroare la write() spre server.\n");
      }
      // wait(NULL);
    }
  }
  else
  {
    size_t marimeMsgPrimit;
    while (1)
    {
      if (read(sd, &marimeMsgPrimit, sizeof(size_t)) > 0)
      {
        char *msgPrimit = (char *)malloc(marimeMsgPrimit);
        bzero(msgPrimit, marimeMsgPrimit);
        if (read(sd, msgPrimit, marimeMsgPrimit) > 0)
        {
          if (strncmp(msgPrimit, "quit", 4) == 0)
          {
            bzero(msgPrimit, marimeMsgPrimit);
            kill(pid, SIGUSR1);
            return 0;
          }
          printf("[client]: %s\n", msgPrimit);
          bzero(msgPrimit, marimeMsgPrimit);
        }
        else {
          error_handler("[client]Eroare la citire mesaj read() din server.\n");
        }
      }
      else {
        error_handler("[client]Eroare la citire dimensiune read() din server.\n");
      }
    }
  }
  close(sd);
  return 0;
}
