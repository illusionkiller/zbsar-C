#ifndef UDP_IPERF2_PROTOCOL_H
#define UDP_IPERF2_PROTOCOL_H

#include <stdint.h>
#include <string.h>

#include "lwip/inet.h"
#include "lwip/sockets.h"

#define UDP_IPERF2_HEADER_SIZE 12U
#define UDP_IPERF2_END_SEQUENCE (-1)

static inline void udp_iperf2_encode_header(uint8_t *packet, int32_t sequence,
                                            const struct timeval *timestamp)
{
    uint32_t value;

    value = htonl((uint32_t)sequence);
    memcpy(packet + 0, &value, sizeof(value));
    value = htonl((uint32_t)timestamp->tv_sec);
    memcpy(packet + 4, &value, sizeof(value));
    value = htonl((uint32_t)timestamp->tv_usec);
    memcpy(packet + 8, &value, sizeof(value));
}

static inline int32_t udp_iperf2_decode_sequence(const uint8_t *packet)
{
    uint32_t value;

    memcpy(&value, packet, sizeof(value));
    return (int32_t)ntohl(value);
}

#endif
