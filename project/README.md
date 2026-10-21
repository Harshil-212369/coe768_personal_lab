# COE768 P2P Project — starter (2026-10-15)

Demo: week of Nov 29 · Report + code to D2L: **Fri Dec 4, 2026** · 150 marks = 15%

## Files

| File | State | What it is |
|---|---|---|
| `pdu.h` | complete | PDU types, field sizes, byte layout, `put_field()` / `get_field()` |
| `index_server.c` | milestone 1 | UDP server from `time_server.c`: receives, prints and dispatches every PDU. The 4 handlers reply "not implemented yet" — **you write them** |
| `peer.c` | not started | Milestones 4–7 |

## Build and test milestone 1 (WSL, ~10 min)

```bash
gcc -Wall -o index_server index_server.c
./index_server 15000                     # terminal 1

# terminal 2: hand-made PDUs with netcat, no peer program needed
printf 'R%-10s%-10s%s' bob song1 127.0.0.1:40123 | nc -u -w1 127.0.0.1 15000; echo
printf 'S%-10s%-10s' alice song1                | nc -u -w1 127.0.0.1 15000; echo
printf 'T%-10s%-10s' bob song1                  | nc -u -w1 127.0.0.1 15000; echo
printf 'O'                                      | nc -u -w1 127.0.0.1 15000; echo
```

`%-10s` pads each name to exactly 10 bytes, which is the fixed field size. Keep using these
commands to test each handler as you write it — the index server is finished before
`peer.c` exists.

## Milestones

| # | Program | Goal | Built from | Demo item |
|---|---|---|---|---|
| 1 | index | UDP loop prints every PDU | `time_server.c` | — |
| 2 | index | Table + R (conflict → E, else A), T, O | new | a, e, f |
| 3 | index | S picks **least-used** server, increments its count | new | b, d |
| 4 | peer | Menu on stdin; UDP socket to index; **R**: TCP socket on port 0 → `getsockname()` → send R → wait A/E | `time_client.c` | a |
| 5 | peer | **Download**: S → connect → D → receive C loop → save → **auto R** | Lab 3 `file_client.c` (DEMO 3c loop) | b, c, d |
| 6 | peer | **Serve**: `select()` on stdin + every listening socket; accept → send C | Lab 3 `file_server.c` (`serve_file()`) | c, d |
| 7 | peer | **O** list; **quit** sends one T per registered content | new | e, f |
| 8 | all | 3 peers + index on lab VMs, Wireshark captures for the report | — | all |

## Design decisions (settle these as a group, they go in the report)

1. **Address encoding** — `pdu.h` uses ASCII `"IP:port"`. Readable in Wireshark, no byte-order bugs.
2. **Which IP the peer registers** — `getsockname()` on the TCP socket returns `0.0.0.0`
   (spec p.6). Two fixes:
   - Peer: `connect()` its UDP socket to the index server (the handout `time_client.c`
     already does), then `getsockname()` on **that** UDP socket → the real interface IP.
   - Index server: ignore the IP in the PDU, use `fsin.sin_addr` from `recvfrom()`.
3. **Index server table** — fixed array of entries
   `{ peer[11], content[11], addr[81], uses, in_use }`. Conflict = same peer AND same content.
4. **O-list limit** — 100 bytes / 10 = max 10 distinct content names per O reply. List each
   content once even if several peers serve it.
5. **Serving downloads** — inline (simple; menu freezes during a transfer) or `fork()` as in
   Lab 3 (child must close the other sockets).
6. **UDP loss** — a lost R or S leaves the peer blocked in `read()`. Fix: receive timeout
   (`SO_RCVTIMEO` or `select()` with a timeout) and resend. Good "future work" material.

## Group split (every member must be able to explain the code in Q/A, 30 marks)

| Person | Owns |
|---|---|
| A | index server (milestones 2–3) |
| B | peer: menu, UDP side, R / O / quit (4, 7) |
| C | peer: TCP download + serve, `select()` (5, 6) — reuses Lab 3 |
| D | test plan, Wireshark, report draft, demo script |
