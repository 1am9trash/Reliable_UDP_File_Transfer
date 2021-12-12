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

#pragma GCC optimize(3)

#define BUF_SIZE 1007
#define WINDOW_SIZE 256

struct PAYLOAD {
    int id;
    int len;
    bool end;
    char data[BUF_SIZE];
};

struct ACK_PAYLOAD {
    int id;
    int nex;
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
        print_info(true, "[INPUT] USAGE: <program> <file name> <port>\n");

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

    FILE *fp = fopen(argv[1], "wb");

    struct PAYLOAD pl[WINDOW_SIZE], recv_pl;
    memset(pl, 0, sizeof(pl));
    bool ack[WINDOW_SIZE];
    memset(ack, 0, sizeof(ack));

    struct sockaddr sender_address;
    socklen_t sender_address_len = sizeof(sender_address);

    print_info(false, "[SYSTEM] INFO: receiver is ready.\n");

    char msg_buf[80];
    int start = 0, send_cnt = 0, recv_cnt = 0;
    ACK_PAYLOAD ack_pl;
    while (true) {
        if (recvfrom(receiver_fd, &recv_pl, sizeof(recv_pl), 0, &sender_address, &sender_address_len) != -1) {
            recv_cnt++;

            send_cnt++;
            ack_pl.id = recv_pl.id;
            if (recv_pl.id == -1) {
                print_info(false, "[SYSTEM] INFO: attempt to connect with sender.\n");
                sendto(receiver_fd, &ack_pl, sizeof(ack_pl), 0, &sender_address, sender_address_len);
                continue;
            }

            int cur = recv_pl.id + 1;
            ack_pl.nex = cur;
            while (cur < start + WINDOW_SIZE && ack[cur % WINDOW_SIZE]) {
                cur++;
                ack_pl.nex = cur;
            }
            sendto(receiver_fd, &ack_pl, sizeof(ack_pl), 0, &sender_address, sender_address_len);

            int recv_mod = recv_pl.id % WINDOW_SIZE;
            if (ack[recv_mod]
                || recv_pl.id < start
                || recv_pl.id >= start + WINDOW_SIZE) {
                sprintf(msg_buf, "[SYSTEM] INFO: receive packet %d.(useless)\n", recv_pl.id);
                print_info(false, msg_buf);
                continue;
            }
            sprintf(msg_buf, "[SYSTEM] INFO: receive packet %d.\n", recv_pl.id);
            print_info(false, msg_buf);
            
            ack[recv_mod] = true;
            pl[recv_mod].id = recv_pl.id;
            pl[recv_mod].len = recv_pl.len;
            pl[recv_mod].end = recv_pl.end;
            memcpy(pl[recv_mod].data, recv_pl.data, recv_pl.len);

            int start_mod = start % WINDOW_SIZE;
            int end_mod = (start + WINDOW_SIZE - 1) % WINDOW_SIZE;
            if (recv_pl.id == start) {
                while (ack[start_mod]) {
                    ack[start_mod] = false;
                    fwrite(pl[start_mod].data, 1, pl[start_mod].len, fp);
                    if (pl[start_mod].end) {
                        int end_cnt = 0;
                        ack_pl.id = start + 1;
                        ack_pl.nex = -1;
                        timeval tv;
                        tv.tv_sec = 1;
                        tv.tv_usec = 0;
                        setsockopt(receiver_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
                        while (end_cnt < 10) {
                            print_info(false, "[SYSTEM] INFO: attempt to disconnect with sender.\n");

                            send_cnt++;
                            sendto(receiver_fd, &ack_pl, sizeof(ack_pl), 0, &sender_address, sender_address_len);
                            if (recvfrom(receiver_fd, &recv_pl, sizeof(recv_pl), 0, &sender_address, &sender_address_len) != -1) {
                                recv_cnt++;
                                end_cnt = 0;
                                if (recv_pl.id == start + 1) {
                                    print_info(false, "[SYSTEM] INFO: disconnect with sender.\n");
                                    break;
                                }
                            } else 
                                end_cnt++;
                        }

                        sprintf(msg_buf, "[STASTIC] INFO: send %d packets.\n", send_cnt);
                        print_info(false, msg_buf);
                        sprintf(msg_buf, "[STASTIC] INFO: receive %d packets.\n", recv_cnt);
                        print_info(false, msg_buf);

                        close(receiver_fd);
                        fclose(fp);
                        return 0;
                    }
                    start++;
                    start_mod = start % WINDOW_SIZE;
                }
            }
        }
    }

    return 0;
}
