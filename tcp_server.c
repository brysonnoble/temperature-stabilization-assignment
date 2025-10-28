#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <math.h>

#define MAX_CLIENTS 4
#define BACKLOG 8
#define EPS 1e-3

struct msg {
    int id;
    double temp;
    int done;
};

ssize_t full_read(int fd, void *buf, size_t count) {
    size_t left = count;
    char *p = buf;
    while (left) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (r == 0) break;
        left -= r;
        p += r;
    }
    return count - left;
}

ssize_t full_write(int fd, const void *buf, size_t count) {
    size_t left = count;
    const char *p = buf;
    while (left) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        left -= w;
        p += w;
    }
    return count;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <initial_central_temp> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    double central = atof(argv[1]);
    int port = atoi(argv[2]);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); exit(EXIT_FAILURE); }
    if (listen(listenfd, BACKLOG) < 0) { perror("listen"); exit(EXIT_FAILURE); }

    printf("Server: listening on port %d, initial central=%.6f\n", port, central);
    int clients[MAX_CLIENTS];
    struct sockaddr_in cliaddr;
    socklen_t cli_len = sizeof(cliaddr);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        int c = accept(listenfd, (struct sockaddr*)&cliaddr, &cli_len);
        if (c < 0) { perror("accept"); exit(EXIT_FAILURE); }
        clients[i] = c;
        char ipstr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cliaddr.sin_addr, ipstr, sizeof(ipstr));
        printf("Accepted client %d from %s:%d -> fd=%d\n", i+1, ipstr, ntohs(cliaddr.sin_port), c);
    }

    double prev_ext[MAX_CLIENTS];
    double cur_ext[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; ++i) prev_ext[i] = 1e100; // large sentinel

    int iteration = 0;
    while (1) {
        iteration++;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            struct msg m;
            ssize_t r = full_read(clients[i], &m, sizeof(m));
            if (r != sizeof(m)) {
                fprintf(stderr, "Error: read from client fd %d returned %zd\n", clients[i], r);
                // continue or exit — choose to exit
                perror("read");
                exit(EXIT_FAILURE);
            }
            if (m.id < 1 || m.id > 4) {
                fprintf(stderr, "Warning: client sent unexpected id %d\n", m.id);
            }
            cur_ext[i] = m.temp;
            printf("Server: recv from client%d: temp=%.6f\n", m.id, m.temp);
        }

        int stable = 1;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (fabs(cur_ext[i] - prev_ext[i]) >= EPS) { stable = 0; break; }
        }

        if (stable) {
            printf("Server: stabilized after %d iterations. final central=%.6f\n", iteration-1, central);
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                struct msg out = {0, central, 1};
                full_write(clients[i], &out, sizeof(out));
                close(clients[i]);
            }
            break;
        }

        double sum = 0.0;
        for (int i = 0; i < MAX_CLIENTS; ++i) sum += cur_ext[i];
        central = (2.0 * central + sum) / 6.0;
        printf("Server: iteration %d -> new central=%.6f\n", iteration, central);

        for (int i = 0; i < MAX_CLIENTS; ++i) {
            struct msg out = {0, central, 0};
            full_write(clients[i], &out, sizeof(out));
        }

        for (int i = 0; i < MAX_CLIENTS; ++i) prev_ext[i] = cur_ext[i];
        // loop
    }

    close(listenfd);
    printf("Server: exiting\n");
    return 0;
}
