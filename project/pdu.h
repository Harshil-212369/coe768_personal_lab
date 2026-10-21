/* pdu.h - COE768 P2P project: PDU format shared by index_server.c and peer.c
 *
 * [Spec II]    "The Type field has the size of one byte."
 * [Spec II]    "The Data field of all the PDU types, except the C-type PDU,
 *               has a maximum size of 100 bytes."
 * [Spec III.1] "we fix the sizes of the peer name and content name fields to
 *               10 bytes each."
 *
 * Byte layout of the Data field. The spec fixes the field ORDER (figures on
 * p.3-4) but not the address encoding; that part is our group's decision.
 *
 *   type  direction            data[0..9]   data[10..19]   data[20..]
 *   ----  -------------------  -----------  -------------  ----------------
 *   R     peer  -> index       peer name    content name   addr "IP:port"
 *   S     peer  -> index       peer name    content name   -
 *   S     index -> peer        peer name    addr "IP:port" (starts at [10])
 *   T     peer  -> index       peer name    content name   -
 *   O     peer  -> index       (empty)
 *   O     index -> peer        content name x N  (10 bytes each, N <= 10)
 *   A     index -> peer        peer name
 *   E     any                  error text
 *   D     peer  -> peer (TCP)  peer name    content name   -
 *   C     peer  -> peer (TCP)  file bytes ... (Lab 3 style, ends at close())
 *
 * Address = ASCII text, e.g. "10.1.1.20:40123". Chosen over binary
 * (4-byte IP + 2-byte port) because it is readable in Wireshark and has no
 * byte-order bugs. Max ADDR_LEN (80) bytes.
 *
 * Name fields are padded with '\0'. A 10-character name fills the field
 * with no terminator, so always copy a field out with get_field().
 */
#ifndef PDU_H
#define PDU_H

#include <string.h>

#define PDU_DATA_MAX	100		/* [Spec II] max Data size (not C) */
#define NAME_LEN	10		/* [Spec III.1] peer / content name */
#define ADDR_LEN	(PDU_DATA_MAX - 2 * NAME_LEN)	/* 80 */

/* Field offsets inside pdu.data */
#define OFF_PEER	0
#define OFF_CONTENT	NAME_LEN		/* 10 */
#define OFF_ADDR_R	(2 * NAME_LEN)		/* 20: address in an R request */
#define OFF_ADDR_S	NAME_LEN		/* 10: address in an S answer  */

/* [Spec II] PDU types (table on p.2) */
#define PDU_REGISTER	'R'	/* content registration           */
#define PDU_DOWNLOAD	'D'	/* content download request (TCP) */
#define PDU_SEARCH	'S'	/* search / search answer         */
#define PDU_DEREGISTER	'T'	/* content de-registration        */
#define PDU_CONTENT	'C'	/* content data (TCP) = Lab 3 'C' */
#define PDU_LIST	'O'	/* list of on-line content        */
#define PDU_ACK		'A'	/* acknowledgement                */
#define PDU_ERROR	'E'	/* error          = Lab 3 'E'     */

/* One UDP datagram = 1 type byte + up to 100 data bytes.
 * Both members are char, so the struct has no padding: sizeof == 101. */
struct pdu {
	char	type;
	char	data[PDU_DATA_MAX];
};

/* put_field - copy C string s into a fixed-width field, pad with '\0'.
 * Returns 0, or -1 if s does not fit (caller should reject the name). */
static inline int put_field(char *field, int width, const char *s)
{
	size_t	n = strlen(s);

	if (n > (size_t)width)
		return -1;
	memset(field, 0, width);
	memcpy(field, s, n);
	return 0;
}

/* get_field - copy a fixed-width field into out (size >= width + 1) as a
 * C string. Stops at the first '\0'. Trailing spaces are dropped so test
 * packets typed by hand with printf '%-10s' work too. */
static inline void get_field(char *out, const char *field, int width)
{
	int	n = 0;

	while (n < width && field[n] != '\0') {
		out[n] = field[n];
		n++;
	}
	while (n > 0 && out[n - 1] == ' ')
		n--;
	out[n] = '\0';
}

#endif /* PDU_H */
