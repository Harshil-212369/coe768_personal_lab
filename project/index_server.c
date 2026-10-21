/* index_server.c - COE768 P2P project: index server (UDP)
 *
 * Base     : time_server.c (UDP handout). socket/bind/recvfrom/sendto are
 *            kept from the handout; the time reply is replaced by a PDU
 *            dispatcher.
 * Usage    : ./index_server <port>
 * Build    : gcc -Wall -o index_server index_server.c
 *
 * MILESTONE 1 (as delivered): receives any PDU, prints its fields, and
 * answers every request with an E-type PDU "... not implemented yet".
 * Next: write the four handlers. Search for "YOUR CODE".
 *
 * Comments tagged [Spec ...] quote P2P_Project2026.pdf.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pdu.h"

#define VERSION		"proj-v0.1-m1"	/* bump this whenever you edit */

static int  parse_port(const char *s);
static void print_pdu(const char *dir, const struct pdu *p, int n,
		      const struct sockaddr_in *who);
static int  send_pdu(int s, const struct sockaddr_in *to, char type,
		     const char *data, int len);
static int  send_error(int s, const struct sockaddr_in *to, const char *msg);

static void handle_register(int s, const struct pdu *req, int n,
			    const struct sockaddr_in *from);
static void handle_search(int s, const struct pdu *req, int n,
			  const struct sockaddr_in *from);
static void handle_deregister(int s, const struct pdu *req, int n,
			      const struct sockaddr_in *from);
static void handle_list(int s, const struct pdu *req, int n,
			const struct sockaddr_in *from);

int main(int argc, char **argv)
{
	struct	sockaddr_in sin;	/* our address                   */
	struct	sockaddr_in fsin;	/* "from" address of each peer   */
	socklen_t alen;			/* handout used int: wrong type   */
	struct	pdu req;
	int	s, port, n;

	printf("index_server %s (built %s %s)\n", VERSION, __DATE__, __TIME__);

	/* No default port: a missing or bad argument is reported, never guessed */
	if (argc != 2) {
		fprintf(stderr, "Usage  : %s <port>\n", argv[0]);
		fprintf(stderr, "Example: %s 15000\n", argv[0]);
		exit(1);
	}
	if ((port = parse_port(argv[1])) < 0) {
		fprintf(stderr, "Bad port \"%s\": must be a number 1024-65535\n",
			argv[1]);
		exit(1);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons(port);

	/* [Spec I] "The communication between the index server and a peer is
	 * based on UDP" -> SOCK_DGRAM */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}
	/* The handout only printed bind errors and carried on; we stop. */
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		fprintf(stderr, "Can't bind to UDP port %d: %s\n", port,
			strerror(errno));
		exit(1);
	}
	/* The handout called listen(s, 5) here. listen() is for TCP
	 * (SOCK_STREAM) only; on a UDP socket it fails with EOPNOTSUPP and
	 * the handout ignored the error. UDP has no connections to queue,
	 * so there is no listen() and no accept(). Removed. */
	printf("[index] 1) Bound to UDP port %d - waiting in recvfrom()\n", port);

	/* One socket serves every peer. recvfrom() tells us who sent each
	 * datagram (fsin); sendto() replies to exactly that address. */
	while (1) {
		memset(&req, 0, sizeof(req));	/* unused bytes read as '\0' */
		alen = sizeof(fsin);
		n = recvfrom(s, &req, sizeof(req), 0,
			     (struct sockaddr *)&fsin, &alen);
		if (n < 0) {
			perror("recvfrom");
			continue;
		}
		if (n == 0)		/* empty datagram: nothing to do */
			continue;
		print_pdu("recv", &req, n, &fsin);

		switch (req.type) {
		case PDU_REGISTER:	handle_register(s, &req, n, &fsin);	break;
		case PDU_SEARCH:	handle_search(s, &req, n, &fsin);	break;
		case PDU_DEREGISTER:	handle_deregister(s, &req, n, &fsin);	break;
		case PDU_LIST:		handle_list(s, &req, n, &fsin);		break;
		default:
			send_error(s, &fsin, "Unknown PDU type");
		}
	}
}

/* ===================== YOUR CODE: the four handlers =====================
 * Each handler: decode fields with get_field(), update / read the content
 * table, reply with send_pdu() or send_error(). You need to design the
 * table first (see README.md, "Index server table").
 */

/* [Spec III.1] "the index server ... will first check if another peer with
 * the same name has registered the same content. If this happens, the index
 * server will send an E-type PDU ... If there is no conflict ... sends back
 * an A-type PDU" */
static void handle_register(int s, const struct pdu *req, int n,
			    const struct sockaddr_in *from)
{
	(void)req; (void)n;
	send_error(s, from, "R not implemented yet");
}

/* [Spec III.2] "the index server chooses a content server which has been
 * used least." Reply: S-type with the address, or E-type if not found. */
static void handle_search(int s, const struct pdu *req, int n,
			  const struct sockaddr_in *from)
{
	(void)req; (void)n;
	send_error(s, from, "S not implemented yet");
}

/* [Spec III.4] "Upon a successful deregistration, the index server will
 * send back an ack (A-type PDU)." */
static void handle_deregister(int s, const struct pdu *req, int n,
			      const struct sockaddr_in *from)
{
	(void)req; (void)n;
	send_error(s, from, "T not implemented yet");
}

/* [Spec III.3] "The index server will respond with an O-type PDU that
 * contains the list of registered contents." Max 10 names per PDU. */
static void handle_list(int s, const struct pdu *req, int n,
			const struct sockaddr_in *from)
{
	(void)req; (void)n;
	send_error(s, from, "O not implemented yet");
}

/* ============================ helpers ================================== */

/* send_pdu - one datagram: 1 type byte + len data bytes */
static int send_pdu(int s, const struct sockaddr_in *to, char type,
		    const char *data, int len)
{
	struct	pdu p;

	if (len < 0 || len > PDU_DATA_MAX)
		return -1;
	memset(&p, 0, sizeof(p));
	p.type = type;
	if (len > 0)
		memcpy(p.data, data, len);
	if (sendto(s, &p, 1 + len, 0, (const struct sockaddr *)to,
		   sizeof(*to)) != 1 + len) {
		perror("sendto");
		return -1;
	}
	print_pdu("sent", &p, 1 + len, to);
	return 0;
}

static int send_error(int s, const struct sockaddr_in *to, const char *msg)
{
	int	len = strlen(msg);

	if (len > PDU_DATA_MAX)
		len = PDU_DATA_MAX;
	return send_pdu(s, to, PDU_ERROR, msg, len);
}

/* print_pdu - one log line per datagram, fields decoded by type */
static void print_pdu(const char *dir, const struct pdu *p, int n,
		      const struct sockaddr_in *who)
{
	char	peer[NAME_LEN + 1], content[NAME_LEN + 1];
	char	addr[ADDR_LEN + 1], text[PDU_DATA_MAX + 1];
	int	dlen = n - 1;		/* data bytes actually received */

	printf("[index] %s %s %s:%d  type '%c'  %d bytes", dir,
	       strcmp(dir, "recv") == 0 ? "from" : "to  ",
	       inet_ntoa(who->sin_addr), ntohs(who->sin_port),
	       p->type, n);

	switch (p->type) {
	case PDU_REGISTER:
		get_field(peer, p->data + OFF_PEER, NAME_LEN);
		get_field(content, p->data + OFF_CONTENT, NAME_LEN);
		get_field(addr, p->data + OFF_ADDR_R, ADDR_LEN);
		printf("  peer=\"%s\" content=\"%s\" addr=\"%s\"",
		       peer, content, addr);
		break;
	case PDU_SEARCH:
		get_field(peer, p->data + OFF_PEER, NAME_LEN);
		if (strcmp(dir, "sent") == 0) {		/* S answer */
			get_field(addr, p->data + OFF_ADDR_S, ADDR_LEN);
			printf("  peer=\"%s\" addr=\"%s\"", peer, addr);
		} else {				/* S request */
			get_field(content, p->data + OFF_CONTENT, NAME_LEN);
			printf("  peer=\"%s\" content=\"%s\"", peer, content);
		}
		break;
	case PDU_DEREGISTER:
		get_field(peer, p->data + OFF_PEER, NAME_LEN);
		get_field(content, p->data + OFF_CONTENT, NAME_LEN);
		printf("  peer=\"%s\" content=\"%s\"", peer, content);
		break;
	case PDU_ACK:
		get_field(peer, p->data + OFF_PEER, NAME_LEN);
		printf("  peer=\"%s\"", peer);
		break;
	case PDU_ERROR:
		memcpy(text, p->data, dlen);
		text[dlen] = '\0';
		printf("  \"%s\"", text);
		break;
	case PDU_LIST:
		printf("  (%d names)", dlen / NAME_LEN);
		break;
	}
	printf("\n");
	fflush(stdout);
}

/* parse_port - returns the port, or -1 if s is not a number in 1024-65535
 * (same as Lab 3 file_server.c) */
static int parse_port(const char *s)
{
	char	*end;
	long	p;

	p = strtol(s, &end, 10);
	if (*s == '\0' || *end != '\0' || p < 1024 || p > 65535)
		return -1;
	return (int)p;
}
