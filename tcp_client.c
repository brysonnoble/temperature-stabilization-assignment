#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

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
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <id 1-4> <initial_temp> <server_ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int id = atoi(argv[1]);
    if (id < 1 || id > 4) {
        fprintf(stderr, "Client id must be 1..4\n");
        exit(EXIT_FAILURE);
    }
    double external = atof(argv[2]);
    char *server_ip = argv[3];
    int port = atoi(argv[4]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); exit(EXIT_FAILURE); }

    struct sockaddr_in srv;
    memset(&srv,0,sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &srv.sin_addr) <= 0) { perror("inet_pton"); exit(EXIT_FAILURE); }

    if (connect(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) { perror("connect"); exit(EXIT_FAILURE); }
    printf("Client%d: connected to %s:%d, initial=%.6f\n", id, server_ip, port, external);

    struct msg out = { id, external, 0 };
    full_write(sock, &out, sizeof(out));

    while (1) {
        struct msg in;
        ssize_t r = full_read(sock, &in, sizeof(in));
        if (r != sizeof(in)) {
            fprintf(stderr, "Client%d: read returned %zd\n", id, r);
            perror("read");
            close(sock);
            exit(EXIT_FAILURE);
        }

        if (in.done) {
            printf("Client%d: DONE received. final stabilized temp = %.6f\n", id, in.temp);
            break;
        } else {
            double central = in.temp;
            external = (3.0 * external + 2.0 * central) / 5.0;
            printf("Client%d: recv central=%.6f -> updated external=%.6f\n", id, central, external);
            struct msg sendm = { id, external, 0 };
            full_write(sock, &sendm, sizeof(sendm));
        }
    }

    close(sock);
    printf("Client%d: exiting\n", id);
    return 0;
}
