#include <sys/select.h>
#include <sys/socket.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

FILE *sock;

void help() {
	fputs(
		"Usage: chat [address] [port] [nick]\n",
		stdout
	);
	exit(0);
}

void quit(int sig) {
	shutdown(fileno(sock), SHUT_RD | SHUT_WR);
	fclose(sock);
	fputs("Disconnected.\n", stdout);
	exit(0);
}

int dial(const char *host, const char *service) {
	int i;
	struct addrinfo hints, *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if (i = getaddrinfo(host, service, &hints, &ai)) {
		fputs(gai_strerror(i), stderr);
		exit(1);
	}
	i = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (connect(i, ai->ai_addr, ai->ai_addrlen)) {
		perror("connect");
		exit(1);
	}
	freeaddrinfo(ai);
	return i;	
}

int main(int argc, const char **argv) {
	fd_set readfds;
	char buf[512];

	if (argc != 4) help();

	signal(SIGINT, &quit);
	setbuf(stdout, 0);
	sock = fdopen(dial(argv[1], argv[2]), "r+");
	setbuf(sock, 0);
	fprintf(sock, "%s\n", argv[3]);
	fputs("Connected.\n", stdout);	
	for (;;) {
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		FD_SET(fileno(sock), &readfds);
		if (select(fileno(sock) + 1, &readfds, 0, 0, 0) == -1) {
			perror("select");
			return 1;
		}
		if (FD_ISSET(0, &readfds)) {
			fgets(buf, 512, stdin);
			fputs(buf, sock);
		} else {
			if (fgets(buf, 512, sock) == NULL) {
				fputs("Disconnected.\n", stdout);
				fclose(sock);
				return 1;
			}
			fputs(buf, stdout);
		}	
	}	
		
}

