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
#define RECV_USEC 1000
#define WINDOW_SIZE 256
#define SS_STEP 10

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
    if (argc != 4)
        print_info(true, "[SYSTEM] USAGE: <program> <file name> <IP> <port>\n");

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

    struct PAYLOAD connect_pl;
    memset(&connect_pl, 0, sizeof(connect_pl));
    connect_pl.id = -1;

    int send_cnt = 0, recv_cnt = 0;

    ACK_PAYLOAD ack_pl;
    ack_pl.id = -2;

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1;
    setsockopt(sender_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    while (true) {
        print_info(false, "[SYSTEM] INFO: attempt to connect with receiver.\n");
        send_cnt++;
        sendto(sender_fd, &connect_pl, sizeof(connect_pl), 0, (struct sockaddr *)&receiver_address, sizeof(receiver_address));
        if (recvfrom(sender_fd, &ack_pl, sizeof(ack_pl), 0, NULL, NULL) != -1 && ack_pl.id == -1) {
            print_info(false, "[SYSTEM] INFO: success to connect with receiver.\n");
            recv_cnt++;
            break;
        }
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(sender_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
    }

    struct stat f_st;
    stat(argv[1], &f_st);
    int f_size = f_st.st_size;
    int package_num = f_size / BUF_SIZE + ((f_size % BUF_SIZE) != 0);

    struct PAYLOAD pl[WINDOW_SIZE];
    memset(pl, 0, sizeof(pl));
    bool ack[WINDOW_SIZE];
    memset(ack, 0, sizeof(ack));

    FILE *fp = fopen(argv[1], "rb");
    char msg_buf[80];
    int start = 0, cur = 0, tv_bias = 1, slow_start = SS_STEP;
    while (true) {
        while (start + WINDOW_SIZE > cur && cur < package_num) {
            send_cnt++;

            int cur_mod = cur % WINDOW_SIZE;
            pl[cur_mod].id = cur;
            pl[cur_mod].end = (cur == (package_num - 1));
            pl[cur_mod].len = fread(pl[cur_mod].data, 1, BUF_SIZE, fp);

            sprintf(msg_buf, "[SYSTEM] INFO: send package %d with tv_usec %d.\n", cur, tv_bias * RECV_USEC);
            print_info(false, msg_buf);
            sendto(sender_fd, &pl[cur_mod], sizeof(pl[cur_mod]), 0, (struct sockaddr *)&receiver_address, sizeof(receiver_address));
            cur++;
        }
        tv.tv_sec = tv_bias * RECV_USEC / 1000000;
        tv.tv_usec = tv_bias * RECV_USEC % 1000000;
        setsockopt(sender_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));

        while (recvfrom(sender_fd, &ack_pl, sizeof(ack_pl), 0, NULL, NULL) != -1) {
            recv_cnt++;

            if (ack_pl.id == package_num && ack_pl.nex == -1) {
                send_cnt++;
                connect_pl.id = package_num;
                sendto(sender_fd, &connect_pl, sizeof(connect_pl), 0, (struct sockaddr *)&receiver_address, sizeof(receiver_address));

                sprintf(msg_buf, "[SYSTEM] INFO: disconnect with receiver.\n");
                print_info(false, msg_buf);

                sprintf(msg_buf, "[STASTIC] INFO: send %d packets.\n", send_cnt);
                print_info(false, msg_buf);
                sprintf(msg_buf, "[STASTIC] INFO: receive %d packets.\n", recv_cnt);
                print_info(false, msg_buf);

                close(sender_fd);
                fclose(fp);
                return 0;
            }

            int recv_mod = ack_pl.id % WINDOW_SIZE;
            if (ack_pl.id == -1
                || ack_pl.id < start 
                || ack_pl.id >= start + WINDOW_SIZE
                || ack[recv_mod]) {
                sprintf(msg_buf, "[SYSTEM] INFO: receive ack %d.(useless)\n", ack_pl.id);
                print_info(false, msg_buf);

                continue;
            }

            sprintf(msg_buf, "[SYSTEM] INFO: receive ack %d.\n", ack_pl.id);
            print_info(false, msg_buf);

            for (int i = ack_pl.id; i < ack_pl.nex; i++)
                ack[i % WINDOW_SIZE] = true;

            if (ack_pl.id == start) {
                int start_mod = start % WINDOW_SIZE;
                while (start < package_num && ack[start_mod]) {
                    ack[start_mod] = false;
                    /*
                    if (pl[start_mod].end) {
                        sprintf(msg_buf, "[STASTIC] INFO: send %d packets.\n", send_cnt);
                        print_info(false, msg_buf);
                        sprintf(msg_buf, "[STASTIC] INFO: receive %d packets.\n", recv_cnt);
                        print_info(false, msg_buf);

                        close(sender_fd);
                        fclose(fp);
                        return 0;
                    }
                    */
                    start++;
                    start_mod = start % WINDOW_SIZE;
                }
            }

            slow_start = SS_STEP;
            if (tv_bias >= 2)
                tv_bias /= 2;
            tv.tv_sec = 0;
            tv.tv_usec = 1;
            setsockopt(sender_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
        }

        if (start + WINDOW_SIZE == cur || (cur == package_num && start < package_num)) {
            if (tv_bias < 200) {
                tv_bias += slow_start;
                slow_start *= 2;
            }

            int start_mod = start % WINDOW_SIZE;
            send_cnt++;
            sendto(sender_fd, &pl[start_mod], sizeof(pl[start_mod]), 0, (struct sockaddr *)&receiver_address, sizeof(receiver_address));

            sprintf(msg_buf, "[SYSTEM] INFO: resend package %d with tv_usec %d.\n", start, tv_bias * RECV_USEC);
            print_info(false, msg_buf);
        }
    }

    sprintf(msg_buf, "[STASTIC] INFO: send %d packets\n", send_cnt);
    print_info(false, msg_buf);
    sprintf(msg_buf, "[STASTIC] INFO: receive %d packets\n", recv_cnt);
    print_info(false, msg_buf);

    close(sender_fd);
    fclose(fp);

    return 0;
}
