#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#define TEMP_FILE "/var/tmp/aesdsocketdata"
#define MAX_READ_SIZE 1024

bool quit = false;
static void signal_handler(int signal_number) { quit = true; }

void create_daemon() {
  pid_t process_id = 0;
  pid_t sid = 0;
  process_id = fork();

  if (process_id < 0) {
    syslog(LOG_ERR, "Fork failed");
    exit(1);
  }
  if (process_id > 0) {
    exit(0);
  }
  umask(0);
  sid = setsid();
  if (sid < 0)
    exit(1);

  chdir("/");
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}

int main(int argc, const char **argv) {
  openlog(NULL, 0, LOG_USER);

  struct sigaction new_action;
  memset(&new_action, 0, sizeof(struct sigaction));
  new_action.sa_handler = signal_handler;
  if (sigaction(SIGTERM, &new_action, NULL) != 0) {
    syslog(LOG_ERR, "%d (%s) registeing for SIGTERM", errno, strerror(errno));
  }
  if (sigaction(SIGINT, &new_action, NULL) != 0) {
    syslog(LOG_ERR, "Error %d (%s) registeing for SIGINT", errno,
           strerror(errno));
  }

  int socketfd;
  if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    syslog(LOG_ERR, "Error create socket %d (%s)", errno, strerror(errno));
    exit(1);
  }
  const int enable = 1;
  if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

  int status;
  struct addrinfo hints;
  struct addrinfo *servinfo;
  struct sockaddr client;

  socklen_t addrlen = sizeof(client);
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0) {
    syslog(LOG_ERR, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }

  if ((status = bind(socketfd, servinfo->ai_addr, sizeof(struct sockaddr))) !=
      0) {
    syslog(LOG_ERR, "bind error: %s\n", gai_strerror(status));
    exit(1);
  }
  freeaddrinfo(servinfo);

  if (argc > 1 && strcmp(argv[1], "-d") == 0) {
    create_daemon();
  }

  listen(socketfd, 1);

  while (!quit) {
    int accept_fd = accept(socketfd, &client, &addrlen);
    if (accept_fd == -1) {
      break;
    }

    struct sockaddr_in *addr_in = (struct sockaddr_in *)(&client);
    syslog(LOG_DEBUG, "Accepted connection from  %s",
           inet_ntoa(addr_in->sin_addr));

    char *buffer = malloc(MAX_READ_SIZE);
    memset(buffer, 0, MAX_READ_SIZE);

    FILE *fp = fopen(TEMP_FILE, "a");
    if (fp == NULL) {
      syslog(LOG_ERR, "Error opening file aesdsocketedata %s", strerror(errno));
      exit(1);
    }

    size_t totalSize = 0;
    size_t readSize = 0;
    char *ret = NULL;

    while (ret == NULL) {
      readSize = recv(accept_fd, buffer + totalSize, MAX_READ_SIZE, 0);
      totalSize += readSize;
      buffer = realloc(buffer, totalSize + MAX_READ_SIZE);
      memset(buffer + totalSize, 0, MAX_READ_SIZE);
      ret = strchr(buffer, '\n');
    }

    fwrite(buffer, 1, totalSize, fp);
    fclose(fp);
    free(buffer);

    fp = fopen(TEMP_FILE, "r");
    if (fp == NULL) {
      syslog(LOG_ERR, "Error opening file aesdsocketedata %s", strerror(errno));
      exit(1);
    }

    char *line = NULL;
    size_t len = 0;
    size_t n;

    while ((n = getline(&line, &len, fp)) != -1) {
      send(accept_fd, line, n, 0);
    }

    fclose(fp);
    free(line);
    shutdown(accept_fd, 2);
    syslog(LOG_DEBUG, "Closed connection from  %s",
           inet_ntoa(addr_in->sin_addr));
  }

  syslog(LOG_DEBUG, "Caught signal, exiting");
  shutdown(socketfd, 2);
  remove(TEMP_FILE);

  return 0;
}
