//
//  Cporxy.c
//  Author:Wenkai Zheng <wenkaizheng@email.arizona.edu>
//          & Zehua Zhang <zehuazhang@email.arizona.edu>
//  Description: Implement TCP proxy program.
//  Copyright @ 2020 Wenkai Zheng and Zehua Zhang. All rights reserved.
//

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>

#define BACKLOG 5
struct sockaddr_in local_addr_info = {0};
struct sockaddr_in remote_addr_indo = {0};
struct sockaddr_in server = {0};
socklen_t socket_length = 0;
char sp[1] = {0};
char telnet[1] = {0};
char heartbeat[6] = {0};
char data[6] = {0};
char data_2[6] = {0};
char confirm[6] = {0};
const char *sip;
int server_port;
int telnet_port;
int s;
int server_socket;
int telnet_socket;
int reconnect = 0;
int change_ip = 0;
int retransfer = 0;
struct timeval count = {0};
struct timeval time_heartbeat_send = {0};
struct timeval time_heartbeat_recv = {0};
struct timeval time_retransfer_send;
struct timeval time_retransfer_current;
struct timeval time_result;
int number = 0;
int sp_number = -1;
int count_time = 0;
int cp_exit = 0;
int network_exp = 0;
typedef struct node
{
    char a[1];
    int ack;
    struct node *next;
    int reset ;
} node;
node *head = NULL;
node *tail = NULL;
int last_send = 0;
int size()
{

    if (head == NULL && tail == NULL)
    {
        return 0;
    }
    int i = 1;
    node *copy = head;
    while (copy != tail)
    {
        i++;
        copy = copy->next;
    }
    return i;
}
void insert(int ack, char *data,int r)
{
    node *n = malloc(sizeof(node));
    n->ack = ack;
    *(n->a) = data[0];
    n->next = NULL;
    n->reset = r;
    if (head == NULL && tail == NULL)
    {
        head = n;
        tail = n;
    }
    if (head == NULL && tail != NULL)
    {
        fprintf(stderr, "Error occur 0\n");
        exit(1);
    }
    if (head != NULL && tail == NULL)
    {
        fprintf(stderr, "Error occur 1\n");
        exit(1);
    }
    // head != null and tail != null
    // head !=null and tail ==null should not occur
    if (head != NULL && tail != NULL)
    {
        tail->next = n;
        tail = n;
    }
}
void pop(node **p)
{

    if (head == NULL && tail == NULL)
    {
        fprintf(stderr, "Error occur 2\n");
        exit(1);
    }
    if (head == NULL && tail != NULL)
    {
        fprintf(stderr, "Error occur 3\n");
        exit(1);
    }
    if (head != NULL && tail == NULL)
    {
        fprintf(stderr, "Error occur 4\n");
        exit(1);
    }
    if (head != NULL && tail != NULL)
    {
        if (head == tail)
        {
            *p = head;
            head = NULL;
            tail = NULL;
        }
        else
        {

            *p = head;
            head = head->next;
        }
    }
}
// t 0 means telnet
// t 1 means server
int recv_all(int socket_id, char *buff, int size)
{
    int rc = recv(socket_id, buff, size, 0);
    if (rc == -1)
    {
        fprintf(stderr, "Recv error\n");
        return -1;
    }
    if (rc == 0)
    {
        return 0;
    }
    if (rc < size)
    {
        int dis = size - rc;
        while (dis != 0)
        {
            buff += rc;
            rc = recv(socket_id, buff, dis, 0);
            if (rc == -1)
            {
                fprintf(stderr, "Recv error\n");
                return -1;
            }
            dis -= rc;
        }
    }
    return 1;
}
int send_all(int socket_id, char *mes, int size)
{
    int rc = send(socket_id, mes, size, MSG_NOSIGNAL);
    if (rc == -1)
    {
        fprintf(stderr, "Send error.\n");
        return -1;
    }
    if (rc < size)
    {
        int dis = size - rc;
        while (dis != 0)
        {
            mes += rc;
            rc = send(socket_id, mes, dis, MSG_NOSIGNAL);
            if (rc == -1)
            {
                fprintf(stderr, "Send error.\n");
                return -1;
            }
            dis -= rc;
        }
    }
    return 1;
}
void clean_up()
{
    bzero(data_2, 6);
    bzero(data, 6);
    bzero(heartbeat, 6);
    bzero(sp, 1);
    bzero(telnet, 1);
    bzero(confirm, 6);
}
void send_message(int flag, node *h)
{
    fd_set writefds;
    // we need to send heart message
    if (flag == 0)
    {
        heartbeat[0] = 'h';
        heartbeat[1] = '0';
        heartbeat[2] = '+';
        heartbeat[3] = '~';
        heartbeat[4] = '~';
        heartbeat[5] = '~';
    }
    else if (flag == 1)
    {

        data_2[0] = 'd';
        data_2[1] = h->ack & 0xff;
        data_2[2] = (h->ack >> 8) & 0xff;
        data_2[3] = (h->ack >> 16) & 0xff;
        data_2[4] = (h->ack >> 24) & 0xff;
        data_2[5] = *(h->a);
        // number++;
    }
    else if (flag == 2)
    {
        confirm[0] = 'a';
        confirm[1] = data[1];
        confirm[2] = data[2];
        confirm[3] = data[3];
        confirm[4] = data[4];
        confirm[5] = '~';
    }
    else if (flag == -1)
    {
        data_2[0] = 'r';
        int j = number - 1;

        data_2[1] = j & 0xff;
        data_2[2] = (j >> 8) & 0xff;
        data_2[3] = (j >> 16) & 0xff;
        data_2[4] = (j >> 24) & 0xff;
        data_2[5] = '~';
    }
    else{
        data_2[0] = 'r';
        data_2[1] = h->ack & 0xff;
        data_2[2] = (h->ack >> 8) & 0xff;
        data_2[3] = (h->ack >> 16) & 0xff;
        data_2[4] = (h->ack >> 24) & 0xff;
        data_2[5] = '~';
    }

    FD_ZERO(&writefds);
    // add our descriptors to the set
    FD_SET(server_socket, &writefds);
    int m = server_socket + 1;
    //printf("158th----%d\n",n);
    struct timeval tv1;
    tv1.tv_sec = 10;
    tv1.tv_usec = 0;
    int rv = select(m, NULL, &writefds, NULL, &tv1);
    if (rv == -1)
    {
        fprintf(stderr, "Select return -1 in 53th\n"); // error occurred in select()
        exit(1);
    }
    else if (rv == 0)
    {
        // not sure but add printf to see
        printf("59\th\n");
    }
    else
    {
        if (FD_ISSET(server_socket, &writefds))
        {
            // we need also send heatbeat message in here
            // send heartbeat message
            if (flag == 0)
            {
                rv = send_all(server_socket, heartbeat, 6);
            }
            else if (flag == 1)
            {
                rv = send_all(server_socket, data_2, 6);
                // printf("Cp send data with %d to Sp\n", h->ack);
            }
            else if (flag == -1)
            {
                rv = send_all(server_socket, data_2, 6);
            }
            else if (flag == -2){
                rv = send_all(server_socket, data_2, 6);
            }
            else
            {

                rv = send_all(server_socket, confirm, 6);
            }

            //   gettimeofday(&time_heartbeat_send, NULL);
            //  send_time = time_heartbeat_send.tv_sec;
            if (rv == -1)
            {
               network_exp = 1;
            }
            if (flag == 0)
            {
                // printf("send heartbead to sp\n");
                gettimeofday(&time_heartbeat_send, NULL);
            }
        }
    }
}
void sp_con (){
    server.sin_family = AF_INET;
    server.sin_port = htons(server_port);
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        fprintf(stderr, "Create server socket failed.\n");
        exit(1);
    }
    int rc = inet_pton(AF_INET, sip, &(server.sin_addr));
    if (rc <= 0)
    {
        fprintf(stderr, "Invalid ip address.\n");
        exit(1);
    }
}
void init()
{
    // init as server for telnet

    if (retransfer == 0)
    {
        node *p = NULL;
        while (head != NULL)
        {
            pop(&p);
        }
    }

    time_heartbeat_recv = (struct timeval){0};
    time_heartbeat_send = (struct timeval){0};
    count = (struct timeval){0};

    if (reconnect == 0 && change_ip == 0)
    {
        s = socket(AF_INET, SOCK_STREAM, 0);
        if (s < 0)
        {
            fprintf(stderr, "Create telnet socket fail");
            exit(1);
        }
        local_addr_info.sin_family = AF_INET;
        local_addr_info.sin_port = htons(telnet_port);
        local_addr_info.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(s, (void *)&local_addr_info, sizeof(local_addr_info)) < 0)
        {
            fprintf(stderr, "106th bind fail\n");
            exit(1);
        }
    }
    if (change_ip == 0)
    {
        if (listen(s, BACKLOG) < 0)
        {
            fprintf(stderr, "listen fail\n");
            exit(1);
        }

        telnet_socket = accept(s, (void *)&remote_addr_indo, &socket_length);
        if (telnet_socket < 0)
        {
            fprintf(stderr, "telnet accpet fail\n");
            exit(1);
        }
    }
    if (cp_exit == 1)
    {
       // printf("Wait for telnet\n");
        if (listen(s, BACKLOG) < 0)
        {
            fprintf(stderr, "listen fail\n");
            exit(1);
        }

        telnet_socket = accept(s, (void *)&remote_addr_indo, &socket_length);
        if (telnet_socket < 0)
        {
            fprintf(stderr, "telnet accpet fail\n");
            exit(1);
        }
    }

    // init as client for server
    sp_con();
    int rc ;
    if (change_ip == 0)
    {
        while (1)
        {
            rc = connect(server_socket, (struct sockaddr *)&server, sizeof(server));
            if (rc != 0)
            {
                close(server_socket);
                sp_con();
                continue;
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        //if (change_ip != 2)
        //{
            while (1)
            {
                //printf("-----------------------------\n");
                rc = connect(server_socket, (struct sockaddr *)&server, sizeof(server));
                if (rc != 0)
                {
                    close(server_socket);
                    sp_con();
                    continue;
                }
                else
                {
                    break;
                }
            }
           
       // }
    }
    if (cp_exit == 1)
    {
        node *p = NULL;
        while (head != NULL)
        {
            pop(&p);
        }
        insert(number++, "A",1);
        send_message(-1, NULL);
        if (network_exp == 1){
            close(server_socket);
            change_ip = 1;
            retransfer = 1;
            clean_up();
            return ;
        }
        gettimeofday(&time_retransfer_send, NULL);
        last_send = number - 1;
    }
    change_ip = 0;
    //cp_exit = 0;
    // sp_exit = 0;
    int n;
    fd_set readfds;
    fd_set writefds;

    while (1)
    {
       // printf("We got something in here\n");
        // clear the set ahead of time
        FD_ZERO(&readfds);
        // add our descriptors to the set
        FD_SET(telnet_socket, &readfds);
        FD_SET(server_socket, &readfds);
        n = telnet_socket > server_socket ? telnet_socket + 1 : server_socket + 1;
        struct timeval tv;
        // set 1ms to get traffic
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int rv = select(n, &readfds, NULL, NULL, &tv);

        if (rv == -1)
        {
            close(telnet_socket);
            close(server_socket);
            fprintf(stderr, "Select return -1 in 169th\n"); // error occurred in select()
            exit(1);
        }
        // timeout send heartbeat
        else if (rv == 0)
        {
            if (time_heartbeat_recv.tv_sec != 0)
            {
                gettimeofday(&count, NULL);
                struct timeval time_result;
                timersub(&count, &time_heartbeat_recv, &time_result);
                if (time_result.tv_sec >= 3)
                {
                    // todo close with server proxy and keep connection with telnet
                    // we will discuss this tomorrow
                    close(server_socket);
                    change_ip = 1;
                    retransfer = 1;
                    clean_up();
                    break;
                }
            }
            send_message(0, NULL);
            if (network_exp == 1){
                 close(server_socket);
                 change_ip = 1;
                 retransfer = 1;
                 clean_up();
                 break;
            }
        }
        else
        {
            // as a client
            // this situation means still connection no change
            //            int flag = 0;
            if (FD_ISSET(server_socket, &readfds))
            {
                //              flag = 1;
                rv = recv_all(server_socket, data, 6);

                if (rv == 0)
                {
                    close(telnet_socket);
                    close(server_socket);
                    reconnect = 1;
                    retransfer = 0;
                    clean_up();
                    break;
                }
                if (rv == -1)
                {
                    close(server_socket);
                    change_ip = 1;
                    retransfer = 1;
                    clean_up();
                    break;
                }
                if (data[0] == 'h' && data[1] == '0' && data[2] == '-')
                {
                    // todo update time in here, no transfer into telnet
                    gettimeofday(&time_heartbeat_recv, NULL);
                    gettimeofday(&count, NULL);
                    struct timeval time_result;
                    timersub(&count, &time_heartbeat_send, &time_result);
                    if (time_result.tv_sec >= 1)
                    {
                        send_message(0, NULL);
                        if (network_exp == 1){
                            close(server_socket);
                            change_ip = 1;
                            retransfer = 1;
                            clean_up();
                            break;
                        }
                    }

                    //  send_heartbeat();
                    clean_up();
                    // continue;
                }
                else if ((data[0] == 'a' && data[5] == '~') || data[0] == 'r')
                {
                    // we get confirm back
                    gettimeofday(&time_heartbeat_recv, NULL);
                    char result[4];
                    result[0] = data[1];
                    result[1] = data[2];
                    result[2] = data[3];
                    result[3] = data[4];
                    int get = *((int *)result);
                    node *pop_result = NULL;
                    if (head == NULL)
                    {
                        
                    }
                    if (head != NULL)
                    {
                        int delete_time = head->ack;
                        int i;
                        for (i = 0; i <= get - delete_time; i++)
                        {
                            pop(&pop_result);
                           
                        }
                    }
                    if (data[0] == 'r')
                    {
                        cp_exit = 0;
                    }
                    // continue;
                }
                
                else
                {
                    send_message(2, NULL);
                    if (network_exp == 1){
                        close(server_socket);
                        change_ip = 1;
                        retransfer = 1;
                        clean_up();
                        break;
                    }
                    sp[0] = data[5];
                    gettimeofday(&time_heartbeat_recv, NULL);
                    FD_ZERO(&writefds);
                    // add our descriptors to the set
                    FD_SET(telnet_socket, &writefds);
                    int m = telnet_socket + 1;
                    struct timeval tv1;
                    tv1.tv_sec = 10;
                    tv1.tv_usec = 0;
                    rv = select(m, NULL, &writefds, NULL, &tv1);
                    if (rv == -1)
                    {
                        fprintf(stderr, "Select return -1 in 255th\n"); // error occurred in select()
                    }
                    else if (rv == 0)
                    {
                       
                        //do nothing
                    }
                    else
                    {

                        if (FD_ISSET(telnet_socket, &writefds))
                        {
                            char result[4];
                            result[0] = data[1];
                            result[1] = data[2];
                            result[2] = data[3];
                            result[3] = data[4];
                            int get = *((int *)result);
                            if (get > sp_number && cp_exit == 0)
                            {
                                rv = send_all(telnet_socket, sp, 1);
                                sp_number = get;
                            }

                            if (rv == -1)
                            {
                                close(server_socket);
                                change_ip = 1;
                                retransfer = 1;
                                clean_up();
                                break;
                            }

                            // send_message(2, NULL);
                        }
                    }
                }
            }
            // one or both of the descriptors have data
            // as a server

            if (FD_ISSET(telnet_socket, &readfds))
            {
                if (time_heartbeat_recv.tv_sec != 0)
                {
                    gettimeofday(&count, NULL);
                    struct timeval time_result;
                    timersub(&count, &time_heartbeat_recv, &time_result);
                    if (time_result.tv_sec >= 3)
                    {
                        // todo close with server proxy and keep connection with telnet
                        // we will discuss this tomorrow
                        close(server_socket);
                        change_ip = 1;
                        retransfer = 1;
                        clean_up();
                        break;
                    }
                }
                // if no traffic from sp, we need to send heartbeat
                rv = recv_all(telnet_socket, telnet, 1);
                // telnet quits
                if (rv == 0 && cp_exit == 0)
                {
                    // we need reconnect
                    cp_exit = 1;

                    //recon_telnet = 1;
                    close(telnet_socket);
                    close(server_socket);
                    change_ip = 1;
                    break;

                    // }
                }
                else if (rv == -1)
                {
                    close(server_socket);
                    change_ip = 1;
                    retransfer = 1;
                    clean_up();
                    break;
                }
                else
                {
                    if (cp_exit == 0)
                    {
                        insert(number++, telnet,0);
                    }
                }
                // send_message(1, tail);
            }
        }
        if (head != NULL && tail != NULL)
        {
            if (count_time >= 1)
            {
                gettimeofday(&time_retransfer_current, NULL);
                timersub(&time_retransfer_current, &time_retransfer_send, &time_result);
                if (head->ack != last_send)
                {
                    if(head->reset == 0){
                    send_message(1, head);
                    }
                    else{
                        send_message(-2,head);
                    }
                    if (network_exp == 1){
                         close(server_socket);
                         change_ip = 1;
                         retransfer = 1;
                         clean_up();
                         break ;
                    }
                    last_send = head->ack;
                    gettimeofday(&time_retransfer_send, NULL);
                }
                else
                {
                    if (time_result.tv_usec > 700)
                    {
                        if(head->reset == 0){
                            send_message(1, head);
                        }
                        else{
                            send_message(-2,head);
                        }
                        if (network_exp == 1){
                             close(server_socket);
                             change_ip = 1;
                             retransfer = 1;
                             clean_up();
                             break;
                        }
                        last_send = head->ack;
                        gettimeofday(&time_retransfer_send, NULL);
                    }
                }
            }
            else
            {
                gettimeofday(&time_retransfer_send, NULL);
                if(head->reset == 0){
                    send_message(1, head);
                }
                else{
                    send_message(-2,head);
                }
                if (network_exp == 1){
                     close(server_socket);
                     change_ip = 1;
                     retransfer = 1;
                     clean_up();
                     break;
                }
                last_send = head->ack;
            }
            count_time++;
        }
        clean_up();
    }
}
int get_port(const char *p)
{
    int i;
    int rc = 0;
    for (i = 0; p[i] != '\0'; i++)
    {
        if (p[i] >= '0' && p[i] <= '9')
        {
            rc = rc * 10 + (p[i] - '0');
        }
        else
        {
            fprintf(stderr, "Invalid port component\n");
            exit(1);
        }
    }
    return rc;
}
int main(int argc, char const *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "You should have two port number and one server ip address\n");
        exit(1);
    }
    sip = argv[2];
    server_port = get_port(argv[3]);
    telnet_port = get_port(argv[1]);
    signal(SIGPIPE, SIG_IGN);
    while (1)
    {
        //printf("^^^^^^^^\n");
        init();
        count_time = 0;
        network_exp = 0;
    }
    
}