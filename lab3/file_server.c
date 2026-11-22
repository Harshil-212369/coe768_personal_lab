/* file_server.c - COE768 Lab 3: file download server based on TCP
 *
 * Base     : echo_server.c (Lab 2 handout). socket/bind/listen/accept/fork
 *            are kept from the handout; echod() is replaced by serve_file().
 * Usage    : ./file_server <port>
 * Build    : gcc -Wall -o file_server file_server.c
 * Fast run : gcc -Wall -DSTEP_DELAY=0 -o file_server file_server.c
 *
 * Search for "DEMO 3a" / "DEMO 3b" to jump to the parts the TA asks about.
 * Comments tagged [Manual ...] quote the Lab 3 manual.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define VERSION		"lab3-v1.0"	/* bump this whenever you edit the file */

/* [Manual II.1] "The maximum number of bytes carried by a packet is 100" */
#define BUFLEN		100

/* [Manual II.3] "the server can insert different characters in the first
 * byte position". Same letters as the P2P project PDU types (C = content,
 * E = error), so this code carries over to the project. */
#define TYPE_CONTENT	'C'
#define TYPE_ERROR	'E'

#ifndef STEP_DELAY
#define STEP_DELAY	3		/* seconds between numbered steps */
#endif
#define MAX_PKT_LINES	10		/* per-packet lines shown before going quiet */

static int step_no = 0;

static void step(const char *fmt, ...);
static int  parse_port(const char *s);
static int  send_packet(int sd, const char *buf, int len, int *npkt);
static int  send_error(int sd, const char *name, const char *reason);
static int  serve_file(int sd);
static void reaper(int sig);

int main(int argc, char **argv)
{
	int	sd, new_sd, port, on = 1;
	socklen_t client_len;
	struct	sockaddr_in server, client;

	printf("file_server %s (built %s %s)\n", VERSION, __DATE__, __TIME__);

	/* No default port: a missing or bad argument is reported, never guessed */
	if (argc != 2) {
		fprintf(stderr, "Usage  : %s <port>\n", argv[0]);
		fprintf(stderr, "Example: %s 5000\n", argv[0]);
		exit(1);
	}
	if ((port = parse_port(argv[1])) < 0) {
		fprintf(stderr, "Bad port \"%s\": must be a number 1024-65535\n",
			argv[1]);
		fprintf(stderr, "Usage  : %s <port>\n", argv[0]);
		exit(1);
	}

	/* Create a stream socket */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Can't create a socket");
		exit(1);
	}
	/* Lets the server restart on the same port right after it is killed */
	if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
		perror("setsockopt(SO_REUSEADDR)");
	step("Socket created (TCP / SOCK_STREAM)");

	/* Bind an address to the socket */
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
		fprintf(stderr, "Can't bind to port %d: %s\n", port,
			strerror(errno));
		exit(1);
	}
	step("Bound to port %d on all interfaces", port);

	/* queue up to 5 connect requests */
	if (listen(sd, 5) == -1) {
		perror("listen");
		exit(1);
	}
	(void) signal(SIGCHLD, reaper);
	step("Listening - blocked in accept(), waiting for a client");

	while (1) {
		client_len = sizeof(client);
		new_sd = accept(sd, (struct sockaddr *)&client, &client_len);
		if (new_sd < 0) {
			if (errno == EINTR)	/* interrupted by SIGCHLD */
				continue;
			perror("Can't accept client");
			exit(1);
		}
		switch (fork()) {
		case 0:		/* child: serves this one client, then exits */
			(void) close(sd);
			step("Client connected from %s:%d (child pid %d)",
			     inet_ntoa(client.sin_addr),
			     ntohs(client.sin_port), (int)getpid());
			exit(serve_file(new_sd));
		default:	/* parent: back to accept() */
			(void) close(new_sd);
			break;
		case -1:
			perror("fork");
			(void) close(new_sd);
		}
	}
}

/* serve_file - read a filename from the client, send the file or an error,
 * then close the connection. Runs in the child process. */
static int serve_file(int sd)
{
	char	name[BUFLEN + 1], buf[BUFLEN];
	struct	stat st;
	int	fd, n, hdr, npkt = 0;
	long	total = 0;

	/* [Manual I] "When the server receives the filename, it will try to
	 * open the requested file." One read(), as in the manual's figure. */
	n = read(sd, name, BUFLEN);
	if (n <= 0) {
		fprintf(stderr, "Client left before sending a filename\n");
		close(sd);
		return 1;
	}
	name[n] = '\0';
	name[strcspn(name, "\r\n")] = '\0';
	step("Filename received: \"%s\"", name);

	/* [Manual I] "if it is not successful, the server will send back an
	 * error message." */
	fd = open(name, O_RDONLY);
	if (fd < 0)
		return send_error(sd, name, strerror(errno));
	if (fstat(fd, &st) == -1 || !S_ISREG(st.st_mode)) {
		close(fd);
		return send_error(sd, name, "Not a regular file");
	}
	step("File opened (%ld bytes) - sending with type byte '%c'",
	     (long)st.st_size, TYPE_CONTENT);

	/* ===== DEMO 3a: how the server transfers the file to the client ====
	 * [Manual I] "since the file size may be greater than the packet
	 * size, the server may need to send multiple packets until all the
	 * data are sent."
	 * hdr = 1 on the first pass: packet = 1 type byte + up to 99 file
	 * bytes (100 total). hdr = 0 afterwards: up to 100 file bytes.
	 * Each pass: read() a chunk from the file, write() it to the socket. */
	buf[0] = TYPE_CONTENT;
	hdr = 1;
	while ((n = read(fd, buf + hdr, BUFLEN - hdr)) > 0) {
		if (send_packet(sd, buf, n + hdr, &npkt) < 0) {
			n = -1;
			break;
		}
		total += n;
		hdr = 0;
	}
	if (n == 0 && hdr == 1)		/* empty file: type byte only */
		if (send_packet(sd, buf, 1, &npkt) < 0)
			n = -1;
	close(fd);

	/* ===== DEMO 3b: when the server stops sending and terminates =======
	 * The loop above ends when read() on the FILE returns 0 (end of
	 * file): nothing is left to send.
	 * [Manual I] "The server closes the TCP connection after the
	 * transmission of the file or error message."
	 * close(sd) makes TCP send a FIN; that FIN is what the client sees
	 * as read() == 0.                                                   */
	if (n < 0)
		fprintf(stderr, "Transfer aborted: %s\n", strerror(errno));
	else
		step("End of file reached: %ld file bytes sent in %d packets",
		     total, npkt);
	close(sd);
	step("Connection closed (FIN sent) - child exits");
	return (n < 0);
}

/* send_error - send one 'E' packet carrying a text message, then close */
static int send_error(int sd, const char *name, const char *reason)
{
	char	buf[BUFLEN];
	int	len;

	len = snprintf(buf, sizeof(buf), "%cError: cannot open \"%s\": %s\n",
		       TYPE_ERROR, name, reason);
	if (len >= (int)sizeof(buf))	/* message was cut to fit one packet */
		len = sizeof(buf) - 1;
	if (write(sd, buf, len) != len)
		perror("write(error message)");
	step("Open failed (%s) - error message sent with type byte '%c'",
	     reason, TYPE_ERROR);
	close(sd);
	step("Connection closed (FIN sent) - child exits");
	return 1;
}

/* send_packet - one write() of at most BUFLEN bytes to the socket */
static int send_packet(int sd, const char *buf, int len, int *npkt)
{
	if (write(sd, buf, len) != len) {
		perror("write(socket)");
		return -1;
	}
	(*npkt)++;
	if (*npkt <= MAX_PKT_LINES)
		printf("      packet %d: %d bytes written\n", *npkt, len);
	else if (*npkt == MAX_PKT_LINES + 1)
		printf("      ... (further packets not listed)\n");
	fflush(stdout);
	return 0;
}

/* step - print a numbered status line, then pause so it can be narrated */
static void step(const char *fmt, ...)
{
	va_list ap;

	printf("[server] %d) ", ++step_no);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
	fflush(stdout);
	sleep(STEP_DELAY);
}

/* parse_port - returns the port, or -1 if s is not a number in 1024-65535 */
static int parse_port(const char *s)
{
	char	*end;
	long	p;

	p = strtol(s, &end, 10);
	if (*s == '\0' || *end != '\0' || p < 1024 || p > 65535)
		return -1;
	return (int)p;
}

/* reaper - collect exited children so they do not stay as zombies */
static void reaper(int sig)
{
	int	saved = errno;

	(void) sig;
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;
	errno = saved;
}
