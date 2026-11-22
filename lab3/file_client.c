/* file_client.c - COE768 Lab 3: file download client based on TCP
 *
 * Base     : echo_client.c (Lab 2 handout). socket/connect are kept from the
 *            handout; the echo loop is replaced by the download logic.
 * Usage    : ./file_client <server_host_or_IP> <port>
 * Build    : gcc -Wall -o file_client file_client.c
 * Fast run : gcc -Wall -DSTEP_DELAY=0 -o file_client file_client.c
 *
 * The download is saved as recv_<filename> in the current directory, so a
 * client started in the server's directory cannot overwrite the original.
 *
 * Search for "DEMO 3c" to jump to the part the TA asks about.
 * Comments tagged [Manual ...] quote the Lab 3 manual.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define VERSION		"lab3-v1.0"	/* bump this whenever you edit the file */

/* [Manual II.1] "The maximum number of bytes carried by a packet is 100" */
#define BUFLEN		100

/* Must match file_server.c */
#define TYPE_CONTENT	'C'
#define TYPE_ERROR	'E'

#define OUT_PREFIX	"recv_"

#ifndef STEP_DELAY
#define STEP_DELAY	3		/* seconds between numbered steps */
#endif
#define MAX_PKT_LINES	10		/* per-read lines shown before going quiet */

static int step_no = 0;

static void step(const char *fmt, ...);
static int  parse_port(const char *s);
static void show_read(int nread, int n);
static int  write_all(int fd, const char *buf, int len);

int main(int argc, char **argv)
{
	int	sd, fd, port, n, nread = 0, status = 0;
	long	total = 0;
	struct	hostent *hp;
	struct	sockaddr_in server;
	char	*host, *base;
	char	name[BUFLEN + 2], buf[BUFLEN];
	char	outname[sizeof(name) + sizeof(OUT_PREFIX)];

	printf("file_client %s (built %s %s)\n", VERSION, __DATE__, __TIME__);

	/* No default port: a missing or bad argument is reported, never guessed */
	if (argc != 3) {
		fprintf(stderr, "Usage  : %s <server_host_or_IP> <port>\n", argv[0]);
		fprintf(stderr, "Example: %s 10.1.1.2 5000\n", argv[0]);
		exit(1);
	}
	host = argv[1];
	if ((port = parse_port(argv[2])) < 0) {
		fprintf(stderr, "Bad port \"%s\": must be a number 1024-65535\n",
			argv[2]);
		fprintf(stderr, "Usage  : %s <server_host_or_IP> <port>\n", argv[0]);
		exit(1);
	}

	/* Resolve the server address (hostname or dotted decimal) */
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if ((hp = gethostbyname(host)) != NULL)
		memcpy(&server.sin_addr, hp->h_addr, hp->h_length);
	else if (inet_aton(host, &server.sin_addr) == 0) {
		fprintf(stderr, "Can't get server's address: \"%s\"\n", host);
		exit(1);
	}

	/* Create a stream socket */
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Can't create a socket");
		exit(1);
	}
	step("Socket created (TCP / SOCK_STREAM)");

	/* Connecting to the server */
	if (connect(sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
		fprintf(stderr, "Can't connect to %s port %d: %s\n", host, port,
			strerror(errno));
		fprintf(stderr, "Is file_server running on that host and port?\n");
		exit(1);
	}
	step("Connected to %s port %d", inet_ntoa(server.sin_addr), port);

	/* [Manual I] "the user on the client side enters the filename to the
	 * terminal, the client retrieves the filename and sends the name to
	 * the server." */
	printf("Enter filename to download: ");
	fflush(stdout);
	if (fgets(name, sizeof(name), stdin) == NULL) {
		fprintf(stderr, "\nNo filename entered\n");
		close(sd);
		exit(1);
	}
	if (strchr(name, '\n') == NULL && strlen(name) > BUFLEN) {
		fprintf(stderr, "Filename too long (max %d characters)\n", BUFLEN);
		close(sd);
		exit(1);
	}
	name[strcspn(name, "\r\n")] = '\0';
	if (name[0] == '\0') {
		fprintf(stderr, "Empty filename\n");
		close(sd);
		exit(1);
	}
	n = strlen(name);
	if (write(sd, name, n) != n) {
		perror("write(filename)");
		close(sd);
		exit(1);
	}
	step("Filename \"%s\" sent (%d bytes) - waiting for the reply", name, n);

	/* [Manual II.3] "When the client receives the first batch of data, it
	 * examines the first byte to determine if the rest of the data is
	 * file data or error message." */
	n = read(sd, buf, BUFLEN);
	if (n < 0) {
		perror("read(socket)");
		close(sd);
		exit(1);
	}
	if (n == 0) {
		fprintf(stderr, "Server closed the connection without replying\n");
		close(sd);
		exit(1);
	}

	if (buf[0] == TYPE_ERROR) {
		/* [Manual II.3] the client "displays the error message in the
		 * terminal window". No local file is created in this case. */
		step("First byte is '%c' -> error message from server:",
		     TYPE_ERROR);
		do {
			if (nread == 0)
				write_all(1, buf + 1, n - 1);
			else
				write_all(1, buf, n);
			nread++;
		} while ((n = read(sd, buf, BUFLEN)) > 0);
		if (n == 0)
			step("read() returned 0: server closed the connection "
			     "-> end of message. No file created");
		status = 1;
	} else if (buf[0] == TYPE_CONTENT) {
		/* [Manual II.3] "The client saves the file data into a local
		 * file". Use only the last path component for the local name. */
		base = strrchr(name, '/');
		base = (base != NULL) ? base + 1 : name;
		snprintf(outname, sizeof(outname), "%s%s", OUT_PREFIX, base);
		fd = open(outname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd < 0) {
			fprintf(stderr, "Can't create \"%s\": %s\n", outname,
				strerror(errno));
			close(sd);
			exit(1);
		}
		step("First byte is '%c' -> file data, saving to \"%s\"",
		     TYPE_CONTENT, outname);

		/* ===== DEMO 3c: when the client stops receiving and exits ==
		 * [Manual I] "the client must call read repeatedly until the
		 * end of transmission. To the client, the closing of the TCP
		 * connection marks the end of the transmission."
		 * read() returns 0 only after the server's close() (FIN) has
		 * arrived and every earlier byte has been delivered. That is
		 * the loop's exit condition. The first batch skips buf[0],
		 * the type byte, which is not part of the file.            */
		do {
			show_read(++nread, n);
			if (nread == 1) {
				status |= write_all(fd, buf + 1, n - 1);
				total += n - 1;
			} else {
				status |= write_all(fd, buf, n);
				total += n;
			}
		} while ((n = read(sd, buf, BUFLEN)) > 0);
		close(fd);
		if (status == 0 && n == 0)
			step("read() returned 0: server closed the connection "
			     "-> end of file. %ld bytes saved in %d reads",
			     total, nread);
	} else {
		fprintf(stderr, "Unknown type byte 0x%02x from server\n",
			(unsigned char)buf[0]);
		status = 1;
	}

	if (n < 0) {
		perror("read(socket)");
		status = 1;
	}
	close(sd);
	step("Socket closed - client exits");
	return status;
}

/* show_read - one line per read() so the 100-byte limit is visible */
static void show_read(int nread, int n)
{
	if (nread <= MAX_PKT_LINES)
		printf("      read %d: %d bytes received\n", nread, n);
	else if (nread == MAX_PKT_LINES + 1)
		printf("      ... (further reads not listed)\n");
	fflush(stdout);
}

/* write_all - write len bytes to fd; returns 0 on success, 1 on failure */
static int write_all(int fd, const char *buf, int len)
{
	int	n;

	while (len > 0) {
		n = write(fd, buf, len);
		if (n < 0) {
			perror("write(local)");
			return 1;
		}
		buf += n;
		len -= n;
	}
	return 0;
}

/* step - print a numbered status line, then pause so it can be narrated */
static void step(const char *fmt, ...)
{
	va_list ap;

	printf("[client] %d) ", ++step_no);
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
