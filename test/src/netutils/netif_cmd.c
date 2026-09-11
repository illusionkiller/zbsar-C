#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "stdbool.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/init.h"
#include "netif/xadapter.h"
#include "shell.h"


extern struct netif net_interface;

/**
 * @brief 显示指定网络接口的详细信息
 * @param netif 网络接口指针
 */
static void show_netif_info(struct netif *netif)
{
    if (netif == NULL)
    {
        return;
    }

    // 打印接口名称
    printf("\nETH_%s:\n", netif->name);
    printf("\tMAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
           netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);

    printf("\tinet addr: %s\n", inet_ntoa(netif->ip_addr)); // inet_ntoa  不可重入
    printf("\tMask: %s\n", inet_ntoa(netif->netmask));
    printf("\tGateway: %s\n", inet_ntoa(netif->gw));

    // 打印接口状态
    printf("\tStatus: %s\n", netif_is_up(netif) ? "UP" : "DOWN");
    printf("\tLink: %s\n", netif_is_link_up(netif) ? "UP" : "DOWN");
}

/**
 * @brief 验证IP地址格式是否正确
 * @param ip IP地址字符串
 * @return 验证结果，成功返回true，失败返回false
 */
static bool validate_ip_address(const char *ip)
{
    if (!ip)
        return false;

    struct in_addr addr;
    return inet_aton(ip, &addr) != 0;
}

/**
 * @brief 设置网络接口参数
 * @param netif 网络接口指针
 * @param ip IP地址
 * @param mask 子网掩码
 * @param gw 网关地址
 * @return 设置结果，成功返回0，失败返回-1
 */
static int set_netif_config(struct netif *netif, const char *ip, const char *mask, const char *gw)
{
    ip_addr_t ipaddr, netmask, gateway;

    // 验证IP地址格式
    if (!validate_ip_address(ip) || !validate_ip_address(mask) || !validate_ip_address(gw))
    {
        printf("Error: Invalid IP address format\n");
        return -1;
    }

    // 转换IP地址
    if (inet_aton(ip, &ipaddr) == 0 ||
        inet_aton(mask, &netmask) == 0 ||
        inet_aton(gw, &gateway) == 0)
    {
        printf("Error: Failed to convert IP address\n");
        return -1;
    }

    // 暂时关闭网络接口
    netif_set_down(netif);

    // 更新IP地址、子网掩码和网关
    netif_set_addr(netif, &ipaddr, &netmask, &gateway);

    // 重新启用网络接口
    netif_set_up(netif);

    setenv("IP", ip,1);
    setenv("mask", mask,1);
    setenv("gateway", gw,1);

    printf("Network interface configuration updated successfully,if need save please use \"saveenv\" command\n");
    return 0;
}

/**
 * @brief ifconfig命令处理函数
 * @param argc 参数数量
 * @param argv 参数列表
 * @return 命令执行结果
 */
static int netif_cmd_handler(int argc, char **argv)
{
    // 显示使用帮助
    if (argc == 1)
    {
        show_netif_info(&net_interface);

        return 0;
    }

    // 检查参数数量
    if (argc == 5)
    {
        // 检查接口名称
        if (strcmp(argv[1], net_interface.name) == 0)
        {
            // 设置网络参数
            if (set_netif_config(&net_interface, argv[2], argv[3], argv[4]) == 0)
            {
                // 显示更新后的配置
                show_netif_info(&net_interface);
                return 0;
            }
            else
            {
                return -1;
            }
        }
        else
        {
            printf("Error: Invalid interface name. Only %s is supported\n", net_interface.name);
            return -1;
        }
    }

    printf("Error: Invalid arguments\n");
    printf("Usage: ifconfig [%s <ip> <mask> <gw>]\n", net_interface.name);
    return -1;
}

/**
 * @brief 注册网络接口命令
 */
void register_netif_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = "ifconfig",
        .help = "Show or configure network interface",
        .hint = NULL,
        .func = &netif_cmd_handler,
    };
    esp_console_cmd_register(&cmd);
}
