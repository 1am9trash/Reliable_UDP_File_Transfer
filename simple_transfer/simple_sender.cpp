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
    if (argc != 4)
        print_info(true, "[SYSYEM] USAGE: <program> <file name> <IP> <port>\n");

    int sender_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sender_fd == -1)
        print_info(true, "[SYSTEM] ERROR: socket() error.\n");

    int port;
    if (!char_to_int(argv[3], &port))
        print_info(true, "[INPUT] ERROR: port must be a positive number.\n");

    struct sockaddr_in receiver_address;
    receiver_address.sin_family = AF_INET;
    receiver_address.sin_port = htons(port);
    inet_aton(argv[2], &receiver_address.sin_addr);

    struct stat f_st;
    stat(argv[1], &f_st);
    int f_size = f_st.st_size;
    int package_num = f_size / BUF_SIZE + ((f_size % BUF_SIZE) != 0);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = RECV_USEC;
    setsockopt(sender_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

    struct INFO info;
    info.id = 0;
    info.package_num = package_num;
    int ack_num = -1;
    sendto(sender_fd, &info, sizeof(info), 0, (struct sockaddr *)&receiver_address, sizeof(receiver_address));

    while (ack_num != 0) {
        if (recvfrom(sender_fd, &ack_num, sizeof(ack_num), 0, NULL, NULL) != -1)
            print_info(false, "[SYSTEM] INFO: receive info ack.\n");
        else {
            print_info(false, "[SYSTEM] INFO: resend info packet.\n");
            sendto(sender_fd, &info, sizeof(info), 0, (struct sockaddr *)&receiver_address, sizeof(receiver_address));
        }
    }

    FILE *fp = fopen(argv[1], "rb");
    struct PAYLOAD pl;
    char msg_buf[80];
    for (int i = 1; i <= package_num; i++) {
        memset(&pl, 0, sizeof(pl));
        pl.id = i;
        pl.len = fread(pl.data, 1, BUF_SIZE, fp);
        sendto(sender_fd, &pl, sizeof(pl), 0, (struct sockaddr *)&receiver_address, sizeof(receiver_address));
        int resend = 0;
        while (ack_num != i) {
            if (recvfrom(sender_fd, &ack_num, sizeof(ack_num), 0, NULL, NULL) != -1) {
                sprintf(msg_buf, "[SYSTEM] INFO: receive ack %d.\n", ack_num);
                print_info(false, msg_buf);
            }
            else {
                resend++;
                if (resend > 20)
                    return 0;
                sprintf(msg_buf, "[SYSTEM] INFO: send packet %d.\n", i);
                print_info(false, msg_buf);
                sendto(sender_fd, &pl, sizeof(pl), 0, (struct sockaddr *)&receiver_address, sizeof(receiver_address));
            }
        }
    }

    close(sender_fd);
    fclose(fp);
    return 0;
}
