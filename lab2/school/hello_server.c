/* Origin: mine, modified from course starter code (Lab 2 echo_server.c) */
/* A simple Hello server using TCP  (COE768 Lab 2, Part III)
 * Modified from echo_server.c: accept ONE client, send "Hello", close, exit. */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define SERVER_TCP_PORT 3000	/* well-known port */
#define MSG		"Hello\n"

int main(int argc, char **argv)
{
	int 	sd, new_sd, port;
	socklen_t client_len;
	struct	sockaddr_in server, client;

	switch(argc){
	case 1:
		port = SERVER_TCP_PORT;
		break;
	case 2:
		port = atoi(argv[1]);
		break;
	default:
		fprintf(stderr, "Usage: %s [port]\n", argv[0]);
		exit(1);
	}

	/* Create a stream socket (TCP) */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	/* Bind an address to the socket */
	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1){
		perror("bind");		/* e.g. "Address already in use" */
		exit(1);
	}

	/* passive mode: queue up to 5 connect requests */
	listen(sd, 5);

	/* Block until ONE client finishes the 3-way handshake */
	client_len = sizeof(client);
	new_sd = accept(sd, (struct sockaddr *)&client, &client_len);
	if (new_sd < 0){
		perror("accept");
		exit(1);
	}

	/* Send the Hello message on the CONNECTED socket (new_sd, not sd) */
	if (write(new_sd, MSG, strlen(MSG)) != (ssize_t)strlen(MSG))
		perror("write");

	close(new_sd);	/* server sends FIN -> starts TCP termination */
	close(sd);	/* stop listening */
	return 0;	/* server exits */
}
