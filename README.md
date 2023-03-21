# Simple NTP Client
This repository contains code for a simple program that queries an NTP server in bursts of 8 requests and computes the delay and offset for each request in those bursts. The server can either be a public NTP server such as `utcnist.colorado.edu` or the toy server present in `ntp_server.c`. The server is hard-coded into `ntp_client.c`, and is the aforementioned public NTP server by default.

To compile and run the program, run `gcc -Wall ntp_client.c -o client` and then `./client`. This will run a program which makes requests in bursts of 8 and records the delay and offset in microseconds of each request in `[server location]_results.txt`.

The file should behave as described in the assignment. The one exception is that the NTP packet does not contain the originate timestamp because the public NTP server was overwriting this value. Instead, the originate timestamp is saved as a variable in the program and used to compute the delay and offset.
