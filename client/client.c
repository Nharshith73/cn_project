#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#define SERVER_PORT 5005
#define BUFFER_SIZE 256

typedef long long ll;

ll simulated_offset_ns;
ll correction_ns = 0;

ll now_ns()
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);

    return (ll)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

ll logical_time()
{
    return now_ns() + simulated_offset_ns + correction_ns;
}

double ns_to_ms(ll value)
{
    return value / 1000000.0;
}

ll absolute_value(ll x)
{
    return x < 0 ? -x : x;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Usage:\n");
        printf("%s <server_ip> <client_id> <offset_ms> <count>\n",
               argv[0]);

        return 1;
    }

    char *server_ip = argv[1];

    int client_id = atoi(argv[2]);

    double offset_ms = atof(argv[3]);

    int count = atoi(argv[4]);

    simulated_offset_ns =
        (ll)(offset_ms * 1000000.0);

    int sockfd;

    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    struct timeval timeout;

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    setsockopt(sockfd,
               SOL_SOCKET,
               SO_RCVTIMEO,
               &timeout,
               sizeof(timeout));

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET,
                  server_ip,
                  &server_addr.sin_addr) <= 0)
    {
        printf("Invalid server IP\n");
        close(sockfd);
        return 1;
    }

    printf("client_id,seq,rtt_ms,estimated_offset_ms,"
           "true_offset_ms,offset_error_ms,post_sync_error_ms\n");

    for (int seq = 1; seq <= count; seq++)
    {
        ll t1 = logical_time();

        char request[BUFFER_SIZE];

        snprintf(request,
                 sizeof(request),
                 "SYNC %d %lld",
                 seq,
                 t1);

        int sent = sendto(sockfd,
                          request,
                          strlen(request),
                          0,
                          (struct sockaddr *)&server_addr,
                          sizeof(server_addr));

        if (sent < 0)
        {
            perror("sendto");
            continue;
        }

        char response[BUFFER_SIZE];

        int n = recvfrom(sockfd,
                         response,
                         sizeof(response) - 1,
                         0,
                         NULL,
                         NULL);

        if (n < 0)
        {
            printf("%d,%d,timeout,timeout,timeout,timeout,timeout\n",
                   client_id,
                   seq);

            usleep(500000);
            continue;
        }

        response[n] = '\0';

        ll t4 = logical_time();

        unsigned int response_seq;

        ll response_t1;
        ll t2;
        ll t3;

        if (sscanf(response,
                   "REPLY %u %lld %lld %lld",
                   &response_seq,
                   &response_t1,
                   &t2,
                   &t3) != 4)
        {
            printf("Invalid response\n");
            continue;
        }

        ll rtt_ns =
            (t4 - t1) - (t3 - t2);

        ll offset_ns =
            ((t2 - t1) + (t3 - t4)) / 2;

        ll current_client_bias =
            simulated_offset_ns + correction_ns;

        ll true_offset_ns =
            -current_client_bias;

        ll offset_error_ns =
            absolute_value(offset_ns - true_offset_ns);

        correction_ns += offset_ns;

        ll remaining_bias =
            simulated_offset_ns + correction_ns;

        ll post_sync_error_ns =
            absolute_value(remaining_bias);

        printf("%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f\n",
               client_id,
               seq,
               ns_to_ms(rtt_ns),
               ns_to_ms(offset_ns),
               ns_to_ms(true_offset_ns),
               ns_to_ms(offset_error_ns),
               ns_to_ms(post_sync_error_ns));

        fflush(stdout);

        usleep(500000);
    }

    close(sockfd);

    return 0;
}
