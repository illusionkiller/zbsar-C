/*
 * Copyright (C) 2016 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */
#include "platform.h"
#include "netif/xadapter.h"
#include "lwip/sys.h"
#include "lwip/init.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#if LWIP_IPV6 == 1
#include "lwip/ip.h"
#else
#if LWIP_DHCP == 1
#include "lwip/dhcp.h"
#endif
#endif

#define DEFAULT_IP "192.168.0.10"
#define DEFAULT_MASK "255.255.255.0"
#define DEFAULT_GW "192.168.0.1"
struct netif net_interface = {
		.name = "0",
};

static void assign_default_ip(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{
    char *ip_str = getenv("IP");
    if (ip_str == NULL)
    {
        ip_str = DEFAULT_IP;
    }
    inet_aton(ip_str, ip);

    char *mask_str = getenv("mask");
    if (mask_str == NULL)
    {
        mask_str = DEFAULT_MASK;
    }
    inet_aton(mask_str, mask);

    char *gw_str = getenv("gateway");
    if (gw_str == NULL)
    {
        gw_str = DEFAULT_GW;
    }
    inet_aton(gw_str, gw);

    /* print out IP settings of the board */

    printf("IP address: %s\r\n", inet_ntoa(*ip));
    printf("Netmask: %s\r\n", inet_ntoa(*mask));
    printf("Gateway: %s\r\n", inet_ntoa(*gw));
    /* print all application headers */
}

static void netif_link_callback(struct netif *netif)
{
    if (netif_is_link_up(netif))
    {
        printf("Network interface is up\r\n");
    }
    else
    {
        printf("Network interface is down\r\n");
    }
}

void network_thread(void *p)
{
    struct netif *netif = (struct netif *)p;
    configASSERT(netif);
    /* the mac address of the board. this should be unique per board */
    char *mac_str = getenv("mac");
    configASSERT(mac_str != NULL);
    unsigned char mac_ethernet_address[6] = {0};
    sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                     &mac_ethernet_address[0], &mac_ethernet_address[1],
                     &mac_ethernet_address[2], &mac_ethernet_address[3],
                     &mac_ethernet_address[4], &mac_ethernet_address[5]);
#if LWIP_IPV6 == 0
    ip_addr_t ipaddr, netmask, gw;
#if LWIP_DHCP == 1
    int mscnt = 0;
#endif
#endif

#if LWIP_IPV6 == 0
#if LWIP_DHCP == 0
    /* initialize IP addresses to be used */
    assign_default_ip(&ipaddr, &netmask, &gw);
#endif


#if LWIP_DHCP == 1
    ipaddr.addr = 0;
    gw.addr = 0;
    netmask.addr = 0;
#endif
#endif

#if LWIP_IPV6 == 0
    /* Add network interface to the netif_list, and set it as default */
    if (!xemac_add(netif, &ipaddr, &netmask, &gw, mac_ethernet_address, PLATFORM_EMAC_BASEADDR))
    {
        printf("Error adding N/W interface\r\n");
        return;
    }
#else
    /* Add network interface to the netif_list, and set it as default */
    if (!xemac_add(netif, NULL, NULL, NULL, mac_ethernet_address, PLATFORM_EMAC_BASEADDR))
    {
        log_error("Error adding N/W interface");
        return;
    }

    netif->ip6_autoconfig_enabled = 1;

    netif_create_ip6_linklocal_address(netif, 1);
    netif_ip6_addr_set_state(netif, 0, IP6_ADDR_VALID);

    printf("Board IPv6 address %x:%x:%x:%x:%x:%x:%x:%x\r\n",
           IP6_ADDR_BLOCK1(&netif->ip6_addr[0].u_addr.ip6),
           IP6_ADDR_BLOCK2(&netif->ip6_addr[0].u_addr.ip6),
           IP6_ADDR_BLOCK3(&netif->ip6_addr[0].u_addr.ip6),
           IP6_ADDR_BLOCK4(&netif->ip6_addr[0].u_addr.ip6),
           IP6_ADDR_BLOCK5(&netif->ip6_addr[0].u_addr.ip6),
           IP6_ADDR_BLOCK6(&netif->ip6_addr[0].u_addr.ip6),
           IP6_ADDR_BLOCK7(&netif->ip6_addr[0].u_addr.ip6),
           IP6_ADDR_BLOCK8(&netif->ip6_addr[0].u_addr.ip6));
#endif

    netif_set_default(netif);
    netif_set_link_callback(netif, netif_link_callback);
    /* specify that the network if is up */
    netif_set_up(netif);

    /* start packet receive thread - required for lwIP operation */
    sys_thread_new("xemacif_input_thread", (void (*)(void *))xemacif_input_thread, netif,
                   1024,
                   DEFAULT_THREAD_PRIO);

#if LWIP_IPV6 == 0
#if LWIP_DHCP == 1
    dhcp_start(netif);
    while (1)
    {
        vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
        dhcp_fine_tmr();
        mscnt += DHCP_FINE_TIMER_MSECS;
        if (mscnt >= DHCP_COARSE_TIMER_SECS * 100)
        {
            dhcp_coarse_tmr();
            mscnt = 0;
        }
    }
#endif
#endif
	vTaskDelete(NULL);
    return;
}

void network_init(void)
{
#if LWIP_DHCP == 1
    int mscnt = 0;
#endif
    struct netif *netif = &net_interface;
    configASSERT(netif);
    /* initialize lwIP before calling sys_thread_new */
    lwip_init();
    /* any thread using lwIP should be created using sys_thread_new */
    sys_thread_new("network_thread", network_thread, netif,
                   configMINIMAL_STACK_SIZE * 2, DEFAULT_THREAD_PRIO - 1);

#if LWIP_IPV6 == 0
#if LWIP_DHCP == 1
    while (1)
    {
        vTaskDelay(DHCP_FINE_TIMER_MSECS / portTICK_RATE_MS);
        if (netif->ip_addr.addr)
        {
            printf("DHCP request success\r\n");
            printf("IP address: %s\r\n", inet_ntoa(netif->ip_addr));
            printf("Netmask: %s\r\n", inet_ntoa(netif->netmask));
            printf("Gateway: %s\r\n", inet_ntoa(netif->gw));
            break;
        }
        mscnt += DHCP_FINE_TIMER_MSECS;
        if (mscnt >= DHCP_COARSE_TIMER_SECS * 100)
        {
            printf("DHCP request timed out\r\n");
            assign_default_ip(&(netif->ip_addr),
                              &(netif->netmask),
                              &(netif->gw));
            break;
        }
    }
#endif
#endif
}
