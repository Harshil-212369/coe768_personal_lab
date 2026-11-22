These are my version of the labs in my home network. 
Goal is to be a network engineer! Cause this subject feels VERY * 10^10 important. 


Disclaimer: Personal study copies, for my own understanding only. Not for submission or reuse in COE768.

## Labs

| Lab | Topic | Folder |
|---|---|---|
| Lab 2 | TCP Hello server and client | [lab2/](lab2/) |
| Lab 3 | TCP file download server and client | [lab3/](lab3/) |
| Lab 5 | RIP network: convergence and failover | [lab5/](lab5/) |

Some labs have a `school/` folder (the version built for the school's Fedora lab VMs) and a `homelab/` folder (my replication on my own network); labs without a homelab version keep their own layout.

Every push is built by GitHub Actions with `gcc -std=gnu89 -Wall -Wextra -Werror` and tested over loopback.
