# Lab 5 study notes

Notes that were useful for this lab: which lectures it rests on, and where the router's behaviour differs from the slides.

## Which lectures matter

| Lecture | Value for Lab 5 | Why |
|---|---|---|
| Lecture 9 Part 2 | Essential | RIP packet format (what you see in Wireshark), the 30 / 180 / 240 s timers, triggered updates, convergence timelines, default route. Steps 13–14 of the manual map almost directly onto the convergence slides. |
| Lecture 9 Part 1 | Essential | Distance vector and Bellman-Ford, count-to-infinity, split horizon. Expect a TA question on why 16 means unreachable. |
| Lecture 8 (both parts) | Support | /24 masks, reading `route -n`, longest prefix match |
| Lab 4 | Support | Lab 5 reuses Lab 4's interface setup and IP forwarding. The static routes are dropped, and RIP learns them instead. |
| Lecture 10 (transport, sliding window) | Not needed | Only one link to it: RIP runs over **UDP port 520**, which has no reliability. That's why RIP resends the whole table every 30 s instead of only changes. |

## Timers: the slides vs Quagga

The slides use Cisco-style timing: invalid at 180 s, a hold-down period, flush at 240 s. Quagga, which the lab VMs run, does it differently:

| Timer | Quagga default | Meaning |
|---|---|---|
| Update | 30 s (± jitter) | Send the full table to neighbours |
| Timeout | 180 s | No update for a route in this long → mark it metric 16 |
| Garbage-collect | 120 s | Keep advertising it at 16 for this long, then delete it |
| Hold-down | **none** | Quagga has no Cisco-style hold-down |

So after a **remote** failure the route disappears about **300 s** after the last update (180 + 120). In between, it's advertised with metric 16. Check on the router with `telnet localhost 2602`, then `show ip rip status`.

A **local** failure (taking your own interface down) skips all of this: the kernel deletes the interface's routes immediately and RIP sends a triggered update.

## Not the same as LeetCode "sliding window"

The transport-layer sliding window (Lecture 10) is a protocol for flow and error control: the window holds frames sent but not yet ACKed, and moves when ACKs arrive. LeetCode's sliding window is an algorithm trick with two indices over an array. They share a name, not content. The part that carries over is modulo arithmetic on a circular buffer: `(NextSeqNum − SendBase) mod N`.

## Sources

- [Quagga docs: RIP Timers](https://nongnu.org/quagga/docs/docs-multi/RIP-Timers.html)
- [INET tutorial: RIP timeout and garbage collection](https://inet.omnetpp.org/docs/tutorials/rip/doc/step4.html)
- RFC 2453, RIP Version 2
