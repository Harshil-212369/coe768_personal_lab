/* lab3_server_once.c - COE768 Lab 3: file download server using TCP
 * Modified from my Lab 2 hello_server.c. Every change is marked "LAB3:".
 *
 * ONE-TIME VERSION of lab3_server.c v1: the server exits by itself after
 * the first SUCCESSFUL file transfer. If a request fails (file not found),
 * it keeps waiting. Differences from lab3_server.c v1 are marked "ONCE:".
 *
 * Build: gcc -Wall -o lab3_server_once lab3_server_once.c
 * Run  : ./lab3_server_once 15000
 */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>	/* LAB3: open(), O_RDONLY */
#include <errno.h>	/* LAB3: errno, for the error message */

#define SERVER_TCP_PORT 3000	/* well-known port */
#define BUFLEN		100	/* LAB3: max bytes per packet (manual II.1) */
#define TYPE_CONTENT	'C'	/* LAB3: first byte = 'C' -> file data follows */
#define TYPE_ERROR	'E'	/* LAB3: first byte = 'E' -> error text follows */

int serve_client(int sd);	/* ONCE: returns 1 = file sent, 0 = not sent */

int main(int argc, char **argv)
{
	int 	sd, new_sd, port, on = 1;
	int	sent;		/* ONCE: 1 if the file was sent successfully */
	socklen_t client_len;
	struct	sockaddr_in server, client;

	printf("lab3_server_once (from v1) - exits after 1 successful transfer (built %s %s)\n", __DATE__, __TIME__);

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
	if (port <= 0 || port > 65535) {
		fprintf(stderr, "Bad port \"%s\"\nUsage: %s [port]\n",
			argv[1], argv[0]);
		exit(1);
	}

	/* Create a stream socket (TCP) */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}
	/* LAB3: lets me restart the server on the same port right away,
	 * instead of getting "Address already in use" for about a minute */
	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

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
	printf("1) Listening on port %d\n", port);

	/* ONCE: keep accepting clients until ONE file has been sent
	 * successfully, then leave the loop and exit. A failed request
	 * (file not found) does not end the server: it waits again. */
	sent = 0;
	while (!sent) {
		/* Block until a client finishes the 3-way handshake */
		client_len = sizeof(client);
		new_sd = accept(sd, (struct sockaddr *)&client, &client_len);
		if (new_sd < 0){
			perror("accept");
			exit(1);
		}
		printf("\n2) Client connected from %s\n",
		       inet_ntoa(client.sin_addr));

		sent = serve_client(new_sd);	/* ONCE: 1 = file sent, 0 = not */

		/* ===== DEMO 3b (second half): terminate the connection =====
		 * Server sends FIN. The client sees it as read() == 0. */
		close(new_sd);
		if (sent)
			printf("6) Connection closed. File sent, so the server exits.\n");
		else
			printf("6) Connection closed. No file sent, waiting for the next client.\n");
	}

	close(sd);	/* ONCE: stop listening */
	return 0;	/* ONCE: server exits after one successful transfer */
}

/* LAB3: everything the server does for one client.
 * It does NOT close sd; main() does that right after this returns.
 * ONCE: returns 1 if the whole file was sent, 0 for any failure. */
int serve_client(int sd)
{
	char	name[BUFLEN + 1];	/* +1 leaves room for the '\0' */
	char	buf[BUFLEN];
	int	fd, n, len, npkt = 0;
	int	ok = 1;		/* ONCE: becomes 0 if sending fails */
	long	total = 0;

	/* Step A: the client speaks first. Read the filename it sent. */
	n = read(sd, name, BUFLEN);
	if (n <= 0) {
		fprintf(stderr, "Client sent no filename\n");
		return 0;		/* ONCE: nothing was sent */
	}
	name[n] = '\0';		/* read() does not add '\0'; I must */
	printf("3) Filename requested: \"%s\"\n", name);

	/* Step B: try to open it. open() works for ANY file, text or binary */
	fd = open(name, O_RDONLY);
	if (fd < 0) {
		/* Failure: one packet = 'E' + a readable message.
		 * ONCE: return 0 so main() knows the file was NOT sent. */
		len = snprintf(buf, BUFLEN, "%cError: cannot open \"%s\": %s\n",
			       TYPE_ERROR, name, strerror(errno));
		if (len >= BUFLEN)	/* message was cut to fit 100 bytes */
			len = BUFLEN - 1;
		if (write(sd, buf, len) != len)
			perror("write");
		printf("4) Open failed -> sent error message (type '%c')\n",
		       TYPE_ERROR);
		return 0;
	}
	printf("4) File opened -> sending (type '%c')\n", TYPE_CONTENT);

	/* ===== DEMO 3a: how the server transfers the file ================
	 * First packet: buf[0] = 'C', then up to 99 file bytes in buf[1..99].
	 * "buf + 1" means "start storing at buf[1]", so buf[0] is kept.   */
	buf[0] = TYPE_CONTENT;
	n = read(fd, buf + 1, BUFLEN - 1);
	if (n >= 0) {
		if (write(sd, buf, n + 1) != n + 1) {
			perror("write");
			ok = 0;
		}
		total = n;
		printf("   packet %d: %d bytes (1 type + %d file)\n",
		       ++npkt, n + 1, n);
	}
	/* Later packets: up to 100 file bytes each. read() returns how many
	 * bytes it really got, so the last packet is usually shorter.     */
	while ((n = read(fd, buf, BUFLEN)) > 0) {
		if (write(sd, buf, n) != n) {
			perror("write");
			ok = 0;
			break;
		}
		total += n;
		printf("   packet %d: %d bytes\n", ++npkt, n);
	}

	/* ===== DEMO 3b (first half): when the server stops sending =======
	 * The while loop ends when read() on the FILE returns 0 = end of
	 * file. Nothing is left to send, so we return and main() closes.  */
	close(fd);
	if (n < 0)		/* read() on the file itself failed */
		ok = 0;
	if (ok)
		printf("5) End of file: %ld file bytes sent in %d packets\n",
		       total, npkt);
	else
		printf("5) Transfer did NOT complete\n");
	return ok;
}

