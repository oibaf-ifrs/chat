#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h> 
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define die(S) { perror(S); exit(1); }

struct client {
	FILE *sock;
	char nick[32];
	struct client *prev, *next;
};

int	server;
struct client *clients;
char	nick[32];

void help() {
	fputs(
		"Usage: chatserver [port] [nick]\n",
		stdout
	);
	exit(0);
}

void quit(int sig) {
	struct client *c, *next;
	for (c = clients; c->next != NULL; c = next) {
		next = c->next;
		if (c->sock != NULL) {
			shutdown(fileno(c->sock), SHUT_RD | SHUT_WR);
			fclose(c->sock);
			free(c);
		}
	}	
	shutdown(server, SHUT_RD | SHUT_WR);
	close(server);
	exit(0);
}

int Listen(const char *service) {
	int i;
	struct addrinfo hints, *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	if (i = getaddrinfo(NULL, service, &hints, &ai)) {
		fputs(gai_strerror(i), stderr);
		exit(1);
	}
	i = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (bind(i, ai->ai_addr, ai->ai_addrlen)) die("bind");
	if (listen(i, 4)) die("listen");
	return i;
}

struct client *remove_client(struct client *c) {
	struct client *next = c->next;
	if (c->next) c->next->prev = c->prev;
	if (c->prev) c->prev->next = c->next;
	shutdown(fileno(c->sock), SHUT_RD | SHUT_WR);
	fclose(c->sock);
	free(c);	
	if (c == clients) clients = next;
	return next;
}

struct client *add_client() {
	struct client *c;
	c = malloc(sizeof(struct client));
	c->prev = NULL;
	c->next = clients;
	if (clients != NULL) clients->prev = c;
	clients = c;
}

void printf_global(struct client *except, const char *format, ...) {
	va_list v;
	struct client *c;
	char client_nick[32];
	va_start(v, format);
	vfprintf(stdout, format, v);
	va_end(v);
	for (c = clients; c != NULL; c = c->next) {
		if (c == except) continue;
		va_start(v, format);
		vfprintf(c->sock, format, v);
		va_end(v);
	}
}

int main(int argc, const char **argv) {
	int i;
	struct client *c, *spoke;
	fd_set readfds;
	char buf[512];
	
	if (argc != 3) help();
	
	clients = NULL;
	strcpy(nick, argv[2]);
	signal(SIGINT, &quit);
	setbuf(stdout, NULL);
	server = Listen(argv[1]);
	
	for (;;) {
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		FD_SET(i = server, &readfds);
		for (c = clients; c != NULL; c = c->next) {
			FD_SET(fileno(c->sock), &readfds);
			if (fileno(c->sock) > i) i = fileno(c->sock);
		}
		if (select(i + 1, &readfds, 0, 0, 0) == -1) die("select");
		if (FD_ISSET(0, &readfds)) {
			fgets(buf, 512, stdin);
			printf_global(0, "%s: %s", nick, buf);
		} else if (FD_ISSET(server, &readfds)) {
			i = accept(server, NULL, NULL);
			if (i == -1) continue;
			c = add_client();
			c->sock = fdopen(i, "r+");
			setbuf(c->sock, NULL);
			fgets(c->nick, 32, c->sock);
			*strchr(c->nick, '\n') = 0;	
			printf_global(0, "%s connected\n", c->nick);
		} else {
			for (c = clients; c != NULL; c = c->next) {
				if (FD_ISSET(fileno(c->sock), &readfds)) {
					spoke = c;
					break;
				}
			}
			if (fgets(buf, 512, spoke->sock) == NULL) {
				printf_global(spoke, "%s disconnected\n", c->nick);
				remove_client(spoke);	
			} else printf_global(spoke, "%s: %s", spoke->nick, buf);	
		}
	}
			
}
