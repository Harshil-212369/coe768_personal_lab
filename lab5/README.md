# Lab 5: Design and Evaluation of a RIP Network

## Objective

Lab 4 had every route typed in by hand with `route add`. In Lab 5 the same Linux routers run RIP (Quagga's `zebra` + `ripd`), learn the routes from their neighbours, and reroute traffic by themselves when a link goes down.

What the demo had to show, on one 4-router cluster:

1. Ping the farthest router in the cluster.
2. Bring down the interface carrying that traffic, and show (tcpdump / Wireshark) that packets move to the other path automatically.
3. Bring it back up, and show the traffic return to the shortest path.

## Topology (cluster 2, my router R5)

Four routers in a ring, with R7 also linked up to the next cluster. Every link is a /24.

```
R5 --10.2.1.0-- R8 --10.2.2.0-- R7 --10.2.3.0-- R6 --10.2.4.0-- R5
                                 \--10.1.6.0-- R2 (uplink)
```

From R5, the target 10.2.2.1 (R7) is 2 hops via R8 (shortest) or 3 hops via R6 (backup).

## Files

| File | What it is | Origin |
|---|---|---|
| [school/lab5_setup.exec](school/lab5_setup.exec) | Manual steps 1–9: interfaces, `zebra.conf`, `ripd.conf`, start both daemons, check `route -n` | AI-assisted, reviewed by me |
| [school/lab5_demo.exec](school/lab5_demo.exec) | Steps 10, 13, 14: one role per terminal (`check`, `capture`, `ping`, `fail`, `recover`, `summary`) | AI-assisted, reviewed by me |
| [school/lab5_R5_evidence.md](school/lab5_R5_evidence.md) | My failover and recovery capture from R5, decoded packet by packet | AI-assisted analysis of my capture |
| [school/class_configs/](school/class_configs/) | Per-router interface setup for R5–R8 (static routes removed) | A classmate's, used with permission |
| [notes.md](notes.md) | Lecture material behind this lab, and Quagga vs slide timers | Study notes |

## Run (school VM, as root)

```bash
su -
bash lab5_setup.exec R5          # once per boot: interfaces, zebra, ripd
bash lab5_demo.exec R5 guide     # prints the 3-terminal demo sequence
```

`lab5_setup.exec` asks you to paste only the `echo 1 > ...` and `/sbin/ifconfig` lines from your router's config. The `route add` lines are Lab 4 static routes, and they would override RIP.

## What I learned

| Concept | What I saw in my capture |
|---|---|
| **Periodic updates** (step 10) | R5 and R6 sent their full tables every **23–36 s, never exactly 30**. Quagga adds random jitter (RFC 2453) so routers don't synchronise. |
| **Split horizon** | R5's updates to R6 never listed 10.2.3.0 or 10.1.6.0, because R5 learned those *from* R6. |
| **Poisoned route + triggered update** (step 13) | The moment eth0 went down, R5 sent 10.2.1.0 and 10.2.2.0 with **metric 16** (unreachable) without waiting for the 30 s timer. |
| **Routing hole** | For about 15 s, ping printed `Network is unreachable`: the kernel had *no* route, so `sendmsg()` failed locally. That differs from a black hole, where packets leave and vanish. |
| **Backup route** | R5 had heard R6 offer 10.2.2.0 at metric 2 earlier but **didn't keep it**, since RIP stores only the best route. It had to wait for R6's next periodic update, then installed the route via R6 at metric 3. |
| **Outage length** | 132 pings sent, 117 received: 15 lost, so about 15 s. It's however long until the neighbour's next periodic update, so anywhere from about 0 to 35 s. |
| **Recovery** (step 14) | eth0 up → R5 advertised 10.2.1.0 at metric 1 straight away, and two seconds later re-learned 10.2.2.0 via R8 at metric 2. ICMP stopped on the backup interface. |
| **Ties don't switch** | After recovery 10.1.6.0 stayed via R6 at metric 3. Via R8 is also 3, and RIP only switches routes for a *smaller* metric. |

### Local vs remote failure

The manual says to ping "for more than RIP flush time", but my failover took 15 s, not 180+. Taking eth0 down is a **local** failure: the kernel deletes the routes on that interface immediately, so no timer is involved. The 180 s timeout and 120 s garbage-collect timers only matter for a **remote** failure, where a neighbour silently stops sending updates. To see them, I'd need to break a link one hop away instead.

## TA questions I prepared

| Question | Answer |
|---|---|
| Why does 16 mean unreachable? | RIP limits paths to 15 hops. Capping "infinity" at 16 stops count-to-infinity from looping forever, at the cost of limiting network size. |
| Why remove `passive-interface eth0` from `ripd.conf`? | Passive means "receive updates but never send them". With the class wiring, eth0 is a ring link on every router, so passive eth0 would split the cluster in two. |
| Why `route del default` before starting RIP? | `redistribute kernel` advertises every kernel route. A leftover default route would be advertised to every other router. |
| Why exactly 15 s of outage? | RIP keeps no backup routes. R5 discarded R6's offer earlier, so it had to wait for R6's next periodic update. R6's triggered update carried only the route that changed at R6 (10.2.1.0), not 10.2.2.0. |
| Why isn't the interval exactly 30 s? | Random jitter, so the routers' updates don't synchronise and collide. |
| What protocol does RIP run on? | UDP port 520, sent to multicast 224.0.0.9 (RIPv2). UDP has no reliability, which is why the full table is resent every period. |

## Homelab replication (planned)

Rebuild the 4-router ring with Linux network namespaces and FRRouting's `ripd` (the maintained successor to Quagga). Then test a **remote** failure, which the school demo didn't cover, and watch the 180 s timeout and 120 s garbage-collect timers run.

## Also feeds

COE865: RIP, OSPF and BGP labs build directly on this.
