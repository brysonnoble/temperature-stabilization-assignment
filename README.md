# Temperature Stabilization Assignment

This repository contains a TCP client-server implementation of the temperature stabilization protocol as described in the assignment.  
Clients represent "external" temperatures, and the server computes a new "central" temperature each iteration, repeating this process until stabilization.

When executed, the program launches a temperature stabilization simulation consisting of one central server and four clients. The server maintains an initial temperature of 50.0°C, while each client starts at different external temperatures (20°C, 30°C, 40°C, and 60°C). The clients periodically exchange temperature data with the server over TCP sockets, allowing the system to iteratively balance all temperatures toward equilibrium.

## Files  
- `tcp_server.c` — server side code  
- `tcp_client.c` — client side code  

## Build Instructions  
In a terminal, compile:  
```bash
gcc -o tcp_server tcp_server.c  
gcc -o tcp_client tcp_client.c  
```

## Output Screenshot
![output screenshot](temps.png)
