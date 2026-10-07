/* Origin: mine, modified from course starter code (Lab 2 echo_client.c) */
/* A simple Hello client using TCP  (COE768 Lab 2, Part III)
 * Modified from echo_client.c: no user input; read until the server closes. */
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#define SERVER_TCP_PORT 3000	/* well-known port */
#define BUFLEN		256	/* buffer length */

int main(int argc, char **argv)
{
	int 	n, sd, port;
	struct	hostent		*hp;
	struct	sockaddr_in server;
	char	*host, rbuf[BUFLEN];

	switch(argc){
	case 2:
		host = argv[1];
		port = SERVER_TCP_PORT;
		break;
	case 3:
		host = argv[1];
		port = atoi(argv[2]);
		break;
	default:
		fprintf(stderr, "Usage: %s host [port]\n", argv[0]);
		exit(1);
	}

	/* Create a stream socket (TCP) */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if ((hp = gethostbyname(host)))
		bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);
	else if (inet_aton(host, &server.sin_addr) == 0){	/* 0 = invalid */
		fprintf(stderr, "Can't get server's address\n");
		exit(1);
	}

	/* 3-way handshake happens inside connect() */
	if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1){
		perror("connect");
		exit(1);
	}

	/* Keep reading until read() returns 0 (server's FIN arrived) */
	while ((n = read(sd, rbuf, BUFLEN)) > 0)
		write(1, rbuf, n);		/* fd 1 = terminal (stdout) */

	if (n < 0)
		perror("read");
	else
		printf("Server closed the connection.\n");

	close(sd);	/* our FIN -> completes the termination */
	return(0);
}
