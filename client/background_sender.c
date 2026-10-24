#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define BG_PORT 6000

long long now_us()
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (long long)ts.tv_sec * 1000000LL
         + ts.tv_nsec / 1000;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Usage:\n");
        printf("%s <server_ip> <rate_mbps> <duration_sec> <packet_size>\n",
               argv[0]);

        return 1;
    }

    char *server_ip = argv[1];

    double rate_mbps = atof(argv[2]);

    int duration = atoi(argv[3]);

    int packet_size = atoi(argv[4]);

    if (packet_size > 1400)
        packet_size = 1400;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BG_PORT);

    inet_pton(AF_INET,
              server_ip,
              &server_addr.sin_addr);

    char *buffer = malloc(packet_size);

    memset(buffer, 'B', packet_size);

    double rate_bps = rate_mbps * 1000000.0;

    double interval_us =
        ((double)packet_size * 8.0 * 1000000.0)
        / rate_bps;

    long long start = now_us();

    long long end =
        start + (long long)duration * 1000000LL;

    long long packets = 0;

    printf("Background traffic started\n");
    printf("Rate = %.2f Mbps\n", rate_mbps);
    printf("Duration = %d seconds\n", duration);

    while (now_us() < end)
    {
        sendto(sockfd,
               buffer,
               packet_size,
               0,
               (struct sockaddr *)&server_addr,
               sizeof(server_addr));

        packets++;

        usleep((unsigned int)interval_us);
    }

    printf("Background packets sent = %lld\n",
           packets);

    free(buffer);

    close(sockfd);

    return 0;
}
