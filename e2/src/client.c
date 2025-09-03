#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  const char *host = "127.0.0.1";
  int port = 5000;
  for (int i = 1; i < argc; i++)
  {
    if (!strcmp(argv[i], "-h") && i + 1 < argc)
      host = argv[++i];
    else if (!strcmp(argv[i], "-p") && i + 1 < argc)
      port = atoi(argv[++i]);
  }
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in sa = {0};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = inet_addr(host);
  if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
  {
    perror("connect");
    return 1;
  }
  char buf[1024];
  int n;
  n = (int)read(fd, buf, sizeof(buf) - 1);
  if (n > 0)
  {
    buf[n] = 0;
    fputs(buf, stdout);
  }

  while (fgets(buf, sizeof(buf), stdin))
  {
    ssize_t sent = write(fd, buf, strlen(buf));
    if (sent < 0)
    {
      perror("write");
      break;
    } // maneja error de envío

    int n = (int)read(fd, buf, sizeof(buf) - 1);
    if (n <= 0)
      break;
    buf[n] = 0;
    fputs(buf, stdout);
    ;
  }
  close(fd);
  return 0;
}
