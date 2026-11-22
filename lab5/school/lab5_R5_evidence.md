# COE768 Lab 5 — RIP failover evidence (R5 Brampton, cluster 2)

Date: 2026-11-20 · Router: R5 (eth0 10.2.1.2 → R8, eth1 10.2.4.1 → R6)
Test: `ping 10.2.2.1` (R7) · capture on **eth1** (backup side):
`tcpdump -s 0 -n -vv -i eth1 udp port 520 or icmp | tee /demo_lab5/fail2.txt`

## Topology (ring)

```
R5 --10.2.1.0-- R8 --10.2.2.0-- R7 --10.2.3.0-- R6 --10.2.4.0-- R5
                                 \--10.1.6.0-- R2 (uplink)
```
Shortest path R5 → 10.2.2.0: via R8 (metric 2). Backup: via R6 → R7 (metric 3).

---

## Step 13 — failure (`/sbin/ifconfig eth0 down` at ~20:54:29)

| Time | Packet (on eth1) | Meaning | RIP phase |
|---|---|---|---|
| 20:54:11.70 | R6 periodic: 10.1.6.0 m2, **10.2.2.0 m2**, 10.2.3.0 m1 | Normal. R6 offers 10.2.2.0, but R5 ignores it (via R8 = 2 is better than via R6 = 3) and **does not keep it** (RIP stores only the best route) | Steady state |
| ping seq 1–61 | no ICMP on eth1 | Pings travel eth0 → R8 | Steady state |
| **20:54:29.58** | **R5 → 10.2.1.0 m16, 10.2.2.0 m16** | eth0 down → R5 flushes eth0 routes, sends **triggered update** with **poison (16)** | Failure detected + bad news sent |
| ping seq 62–76 | `sendmsg: Network is unreachable` ×15 | R5 has **no route** to 10.2.2.0 | **Routing hole** |
| 20:54:33.58 | R6 triggered: 10.2.1.0 **m3** | R6 lost its route via R5, found one via R7 → R8, told R5 (R5 now has 10.2.1.0 via R6, metric 4) | Neighbour re-converges |
| 20:54:36.26 | R5 triggered: 10.2.2.0 m16 | 10.2.2.0 still unreachable at R5. 10.2.1.0 is no longer poisoned toward R6 (learned from R6, so split horizon) | Bad news repeated |
| **20:54:44.69** | **R6 periodic: 4 routes incl. 10.2.2.0 m2** | R5 finally re-hears 10.2.2.0 → installs via R6, **metric 3** | Backup route learned |
| **20:54:45.29** | **First ICMP on eth1 (seq 77) + reply** | 0.6 s after R6's update, traffic flows via R6 | **Sub-optimal route** in use |

**Result:** 132 transmitted, 117 received, **11 % loss = 15 pings ≈ 15 s outage**.

### Why exactly 15 s (TA question)
R5 deleted its eth0 routes instantly (local failure detection), but RIP **does not store backup routes**.
It had heard R6's 10.2.2.0 at 20:54:11 and discarded it as worse. So R5 had to wait for **R6's next
periodic update** (20:54:44.69). R6's triggered update at 20:54:33 carried only the route that
*changed at R6* (10.2.1.0), not 10.2.2.0. Outage = time until the neighbour's next 30 s update
(here 15 s; could be anywhere from ~0 to ~35 s).

### Why "Network is unreachable" (not silent loss)
The kernel had **no matching route**, so ping's `sendmsg()` failed locally. This is the
**routing hole** phase (Lecture 9 Part 1, slide 25), not a black hole where packets leave and vanish.

---

## Step 14 — recovery (`/sbin/ifconfig eth0 up`, earlier run at ~20:26:13)

| Time | Packet (on eth1) | Meaning |
|---|---|---|
| 20:26:13.51 | R5 triggered: **10.2.1.0 m1** | eth0 back; directly connected again |
| 20:26:13.82 | last ICMP on eth1 (seq 2381) | traffic leaves the backup path |
| 20:26:15.51 | R5 triggered: **10.2.2.0 m2** | R5 re-learned 10.2.2.0 via R8 (metric 2) → optimal route restored |
| after | no more ICMP on eth1 | pings back on eth0 → R8 |

`route -n` after `eth0 up`: 10.2.2.0 via 10.2.1.1 (R8), metric 2. traceroute hop 1 = 10.2.1.1.
10.1.6.0 stayed via R6 (metric 3): **tie** with via R8, and RIP only switches for a *smaller* metric.

---

## Step 10 — RIP interval (periodic updates)

| Router | Update times | Gaps (s) |
|---|---|---|
| R5 (10.2.4.1) | 20:13:31, 20:13:54, 20:14:28, 20:14:57, 20:15:22 | 23, 34, 29, 25 |
| R5 (10.2.4.1) | 20:26:37 → 20:30:14 (7 gaps) | 27, 27, 28, 29, 35, 36, 35 |
| R6 (10.2.4.2) | 20:25:04 → 20:29:47 (9 gaps) | 35, 35, 33, 35, 23, 30, 30, 30, 32 |

**≈ 30 s, never exactly 30.** Quagga adds random jitter (RFC 2453) so routers don't synchronise.

## Split horizon (seen throughout)
- R5's updates to R6 contain only 10.2.1.0 and 10.2.2.0, never 10.2.3.0 / 10.1.6.0 (learned **from** R6).
- R6's updates omit 10.2.1.0 while R6 learns it from R5; it appears (m3) only while R5's eth0 is down.

## Timers (Quagga vs slides)
Quagga: update 30 s, timeout 180 s, garbage-collect 120 s, **no hold-down**.
Slides (Cisco-style): flush 240 s, hold-down. Verify on the router: `telnet localhost 2602` → `show ip rip status`.

---

## Raw extract — fail2.txt lines 113–142

```
20:54:11.700845 10.2.4.2.router > 224.0.0.9.router: RIPv2, Response, routes: 3
	  AFI: IPv4: 10.1.6.0/24, metric: 2
	  AFI: IPv4: 10.2.2.0/24, metric: 2
	  AFI: IPv4: 10.2.3.0/24, metric: 1
20:54:29.577471 10.2.4.1.router > 224.0.0.9.router: RIPv2, Response, routes: 2
	  AFI: IPv4: 10.2.1.0/24, metric: 16
	  AFI: IPv4: 10.2.2.0/24, metric: 16
20:54:33.577771 10.2.4.2.router > 224.0.0.9.router: RIPv2, Response, routes: 1
	  AFI: IPv4: 10.2.1.0/24, metric: 3
20:54:36.259626 10.2.4.1.router > 224.0.0.9.router: RIPv2, Response, routes: 1
	  AFI: IPv4: 10.2.2.0/24, metric: 16
20:54:44.691813 10.2.4.2.router > 224.0.0.9.router: RIPv2, Response, routes: 4
	  AFI: IPv4: 10.1.6.0/24, metric: 2
	  AFI: IPv4: 10.2.1.0/24, metric: 3
	  AFI: IPv4: 10.2.2.0/24, metric: 2
	  AFI: IPv4: 10.2.3.0/24, metric: 1
20:54:45.285419 10.2.4.1 > 10.2.2.1: ICMP echo request, seq 77
20:54:45.288242 10.2.2.1 > 10.2.4.1: ICMP echo reply, seq 77   (ttl 63 = one router, R6)
```
(IP/UDP header fields and hex dumps trimmed for readability.)
