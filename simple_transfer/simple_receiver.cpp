#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
using namespace std;

#define BUF_SIZE 1008
#define RECV_USEC 1000

struct INFO {
    int id;
    int package_num;
};

struct PAYLOAD {
    int id;
    int len;
    char data[BUF_SIZE];
};

void print_info(bool error, const char *msg) {
    if (!error)
        fprintf(stdout, "%s", msg);
    else {
        fprintf(stderr, "%s", msg);
        exit(1);
    }
}

bool char_to_int(char *c, int *p) {
    *p = 0;
    for (int i = 0; i < strlen(c); i++) {
        *p *= 10;
        if (c[i] < '0' || c[i] > '9')
            return false;
        *p += c[i] - '0';
    }
    return true;
}

int main(int argc, char *argv[]) {
    if (argc != 3)
        print_info(true, "[SYSTEM] USAGE: <program> <file name> <port>\n");

    int receiver_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver_fd == -1)
        print_info(true, "[SYSTEM] ERROR: socket() error.\n");

    int port;
    if (!char_to_int(argv[2], &port))
        print_info(true, "[INPUT] ERROR: port must be a positive number.\n");

    struct sockaddr_in receiver_address;
    receiver_address.sin_family = AF_INET;
    receiver_address.sin_port = htons(port);
    inet_aton("0.0.0.0", &receiver_address.sin_addr);

    if (bind(receiver_fd, (struct sockaddr *)&receiver_address, sizeof(receiver_address)) == -1)
        print_info(true, "[SYSTEM] ERROR: bind() error.\n");

    struct sockaddr sender_address;
    socklen_t sender_address_len = sizeof(sender_address);
    struct INFO info;
    int ack_num = 0;
    recvfrom(receiver_fd, &info, sizeof(info), 0, &sender_address, &sender_address_len);
    sendto(receiver_fd, &ack_num, sizeof(ack_num), 0, &sender_address, sender_address_len);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = RECV_USEC;
    setsockopt(receiver_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

    int cur = 1;
    FILE *fp = fopen(argv[1], "wb");
    struct PAYLOAD pl;
    pl.id = 0;

    char msg_buf[80];
    while (cur <= info.package_num) {
        if (recvfrom(receiver_fd, &pl, sizeof(pl), 0, &sender_address, &sender_address_len) != -1) {
            sprintf(msg_buf, "[SYSTEM] INFO: receive packet %d.\n", pl.id);
            print_info(false, msg_buf);
            sprintf(msg_buf, "[SYSTEM] INFO: send ack %d.\n", pl.id);
            print_info(false, msg_buf);
            sendto(receiver_fd, &(pl.id), sizeof(pl.id), 0, &sender_address, sender_address_len);
            if (pl.id == cur) {
                fwrite(pl.data, 1, pl.len, fp);
                cur++;
            }
        }
    }

    close(receiver_fd);
    fclose(fp);
    return 0;
}

