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
#include <pthread.h>
#include <time.h>

#define TEMP_FILE "/var/tmp/aesdsocketdata"
#define MAX_READ_SIZE 1024

bool quit = false;
static struct thread_data head;
static struct thread_data *tail;
static pthread_mutex_t mutex;

struct thread_data {
	pthread_mutex_t *mutex;
	int socket;
	pthread_t thread;
	bool is_completed;
	struct thread_data *next;
};

static void signal_handler(int signal_number)
{
	quit = true;
}

void *process(void *params)
{
	struct thread_data *p = (struct thread_data *)params;

	char *buffer = malloc(MAX_READ_SIZE);
	memset(buffer, 0, MAX_READ_SIZE);

	FILE *fp = fopen(TEMP_FILE, "a");
	if (fp == NULL) {
		syslog(LOG_ERR, "Error opening file aesdsocketedata %s",
		       strerror(errno));
		exit(1);
	}

	size_t totalSize = 0;
	size_t readSize = 0;
	char *ret = NULL;

	while (ret == NULL) {
		readSize =
			recv(p->socket, buffer + totalSize, MAX_READ_SIZE, 0);
		totalSize += readSize;
		buffer = realloc(buffer, totalSize + MAX_READ_SIZE);
		memset(buffer + totalSize, 0, MAX_READ_SIZE);
		ret = strchr(buffer, '\n');
	}

	pthread_mutex_lock(p->mutex);
	fwrite(buffer, 1, totalSize, fp);
	fclose(fp);
	pthread_mutex_unlock(p->mutex);
	free(buffer);

	fp = fopen(TEMP_FILE, "r");
	if (fp == NULL) {
		syslog(LOG_ERR, "Error opening file aesdsocketedata %s",
		       strerror(errno));
		exit(1);
	}

	char *line = NULL;
	size_t len = 0;
	size_t n;

	while ((n = getline(&line, &len, fp)) != -1) {
		send(p->socket, line, n, 0);
	}

	fclose(fp);
	free(line);
	p->is_completed = true;
	return params;
}

struct thread_data *create_thread(int socket, pthread_mutex_t *mutex)
{
	struct thread_data *td = malloc(sizeof(struct thread_data));
	td->socket = socket;
	td->mutex = mutex;
	td->is_completed = false;
	td->next = NULL;
	tail->next = td;
	tail = td;
	int rc = pthread_create(&td->thread, NULL, process, td);
	if (rc != 0)
		return NULL;
	return td;
}

void create_daemon()
{
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
void create_signals()
{
	struct sigaction new_action;
	memset(&new_action, 0, sizeof(struct sigaction));
	new_action.sa_handler = signal_handler;
	if (sigaction(SIGTERM, &new_action, NULL) != 0) {
		syslog(LOG_ERR, "%d (%s) registeing for SIGTERM", errno,
		       strerror(errno));
	}
	if (sigaction(SIGINT, &new_action, NULL) != 0) {
		syslog(LOG_ERR, "Error %d (%s) registeing for SIGINT", errno,
		       strerror(errno));
	}
}
void timer_thread(union sigval arg)
{
	pthread_mutex_lock(&mutex);
	FILE *fp = fopen(TEMP_FILE, "a");
	if (fp == NULL) {
		syslog(LOG_ERR, "Error opening file aesdsocketedata %s",
		       strerror(errno));
		exit(1);
	}
	char buffer[26];
	time_t timer = time(NULL);
	struct tm *tmp = localtime(&timer);
	strftime(buffer, 26, "%a, %d %b %Y %T %z", tmp);
	fprintf(fp, "timestamp:%s\n", buffer);
	fclose(fp);
	pthread_mutex_unlock(&mutex);
}

timer_t create_timer()
{
	timer_t timer_id;
	struct sigevent se;

	se.sigev_notify = SIGEV_THREAD;
	se.sigev_value.sival_ptr = &timer_id;
	se.sigev_notify_function = timer_thread;
	se.sigev_notify_attributes = NULL;

	if (timer_create(CLOCK_REALTIME, &se, &timer_id) != 0) {
		syslog(LOG_ERR, "Error %d (%s) timer create", errno,
		       strerror(errno));
		exit(1);
	} else {
		struct itimerspec sleep_time;
		int status;

		sleep_time.it_value.tv_sec = 10;
		sleep_time.it_value.tv_nsec = 0;
		sleep_time.it_interval.tv_sec = 10;
		sleep_time.it_interval.tv_nsec = 0;

		status = timer_settime(timer_id, 0, &sleep_time, 0);
		if (status == -1) {
			syslog(LOG_ERR, "Error %d (%s) set time", errno,
			       strerror(errno));
			exit(1);
		}
	}
	return timer_id;
}
void check_threads()
{
	struct thread_data *curr = head.next;
	struct thread_data *prev = &head;

	while (curr != NULL) {
		if (curr->is_completed) {
			int rc = pthread_join(curr->thread, NULL);
			if (rc != 0) {
				syslog(LOG_ERR, "Error %d (%s) thread join",
				       errno, strerror(errno));
				exit(1);
			}
			shutdown(curr->socket, 2);
			if (curr == tail) {
				tail = prev;
			}
			prev->next = curr->next;
			free(curr);
			curr = prev->next;
		} else {
			prev = curr;
			curr = curr->next;
		}
	}
}
int main(int argc, const char **argv)
{
	openlog(NULL, 0, LOG_USER);
	create_signals();

	int socketfd;
	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		syslog(LOG_ERR, "Error create socket %d (%s)", errno,
		       strerror(errno));
		exit(1);
	}
	const int enable = 1;
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &enable,
		       sizeof(int)) < 0)
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
		syslog(LOG_ERR, "getaddrinfo error: %s\n",
		       gai_strerror(status));
		exit(1);
	}

	if ((status = bind(socketfd, servinfo->ai_addr,
			   sizeof(struct sockaddr))) != 0) {
		syslog(LOG_ERR, "bind error: %s\n", gai_strerror(status));
		exit(1);
	}
	freeaddrinfo(servinfo);

	if (argc > 1 && strcmp(argv[1], "-d") == 0) {
		create_daemon();
	}
	int rc = pthread_mutex_init(&mutex, NULL);
	if (rc != 0) {
		exit(1);
	}
	timer_t timer_id = create_timer(&mutex);

	listen(socketfd, 1);

	memset(&head, 0, sizeof(struct thread_data));
	tail = &head;

	while (!quit) {
		int accept_fd = accept(socketfd, &client, &addrlen);
		if (accept_fd == -1) {
			break;
		}

		struct sockaddr_in *addr_in = (struct sockaddr_in *)(&client);
		syslog(LOG_DEBUG, "Accepted connection from  %s",
		       inet_ntoa(addr_in->sin_addr));
		create_thread(accept_fd, &mutex);
		check_threads();
	}

	check_threads();
	timer_delete(timer_id);
	syslog(LOG_DEBUG, "Caught signal, exiting");
	shutdown(socketfd, 2);
	remove(TEMP_FILE);

	return 0;
}
