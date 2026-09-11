#include "platform.h"
#include "pwmdev.h"
#include "shell.h"


// 定义一个结构体来存放所有命令的参数解析表
static struct
{
    struct arg_lit *info;     // pwm -i <info>
    struct arg_str *device;   // pwm -d <device> (pwm_0, pwm_1)
    struct arg_int *channel;   // pwm -c <channel>(0-15)
    struct arg_int *period;     // pwm -p <period>  (0-999999)
    struct arg_int *duty;     // pwm -y <duty>  (0-999999)
    struct arg_lit *enable;     // pwm -e <enable> 
    struct arg_lit *disable;     // pwm -b <disable> 
    struct arg_lit *one_pulse;     // pwm -o <one_pulse>
    struct arg_lit *continuous;     // pwm -t <continuous>
    struct arg_end *end;
} pwm_args;

void show_pwm_help(void)
{
    const char *help_str =
        "Examples:\n"
        "  # Print PWM device information\n"
        "  pwm -i\n"
        "\n"
        "  # Set PWM period\n"
        "  pwm -d pwm_0 -c 0 -p 100000\n"
        "\n"
        "  # Set PWM duty cycle\n"
        "  pwm -d pwm_0 -c 0 -y 50000\n"
        "\n"
        "  # Enable PWM channel\n"
        "  pwm -d pwm_0 -c 0 -e\n"
        "\n"
        "  # Disable PWM channel\n"
        "  pwm -d pwm_0 -c 0 -b\n"
        "\n"
        "  # Set one pulse mode\n"
        "  pwm -d pwm_0 -c 0 -o\n"
        "\n"
        "  # Set continuous wave mode\n"
        "  pwm -d pwm_0 -c 0 -t\n";

    printf("%s", help_str);
}

static int pwm_cmd_handler(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&pwm_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, pwm_args.end, argv[0]);
        return 1;
    }
    if (pwm_args.info->count)
    {
        print_pwm_dev_info();
        return 0;
    }
    
    // 检查必要参数
    if (!pwm_args.device->count || !pwm_args.channel->count)
    {
        printf("Error: Device name and channel ID are required\n");
        show_pwm_help();
        return 1;
    }
    
    const char *device_name = pwm_args.device->sval[0];
    int channel = pwm_args.channel->ival[0];
    
    // 检查参数范围
    if (channel < 0 || channel > PWM_MAX_CHANNEL_NUM)
    {
        printf("Error: Invalid channel ID %d. Must be between 0 and %d.\n", channel, PWM_MAX_CHANNEL_NUM);
        return 1;
    }
    
    // 打开PWM设备
    int dev_id = pwm_dev_open((char *)device_name, 0);
    if (dev_id < 0)
    {
        printf("Error: Failed to open PWM device '%s'\n", device_name);
        return 1;
    }
    
    // 准备PWM数据结构
    pwm_dev_data_t pwm_data;
    pwm_data.channel = channel;
    
    // 处理各种命令
    int command_executed = 0;
    
    // 设置周期
    if (pwm_args.period->count)
    {
        u32 period = pwm_args.period->ival[0];
        if (period < 0 || period > PWM_CHANNEL_MAX_CLOCK)
        {
            printf("Error: Invalid period %lu. Must be between 0 and %d.\n", period, PWM_CHANNEL_MAX_CLOCK);
            pwm_dev_close(dev_id);
            return 1;
        }
        pwm_data.value = period;
        pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_SET_PERIOD, &pwm_data);
        printf("Set PWM period for %s channel %d to %lu\n", device_name, channel, period);
        command_executed = 1;
    }
    
    // 设置占空比
    if (pwm_args.duty->count)
    {
        u32 duty = pwm_args.duty->ival[0];
        if (duty < 0 || duty > PWM_CHANNEL_MAX_CLOCK)
        {
            printf("Error: Invalid duty cycle %lu. Must be between 0 and %d.\n", duty, PWM_CHANNEL_MAX_CLOCK);
            pwm_dev_close(dev_id);
            return 1;
        }
        pwm_data.value = duty;
        pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_SET_DUTY, &pwm_data);
        printf("Set PWM duty cycle for %s channel %d to %lu\n", device_name, channel, duty);
        command_executed = 1;
    }
    
    // 使能PWM
    if (pwm_args.enable->count)
    {
        pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ENABLE, &pwm_data);
        printf("Enabled PWM channel %d on %s\n", channel, device_name);
        command_executed = 1;
    }
    
    // 禁用PWM
    if (pwm_args.disable->count)
    {
        pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_DISABLE, &pwm_data);
        printf("Disabled PWM channel %d on %s\n", channel, device_name);
        command_executed = 1;
    }
    
    // 设置单脉冲模式
    if (pwm_args.one_pulse->count)
    {
        pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ONE_PULSE_MODE, &pwm_data);
        printf("Set PWM channel %d on %s to one pulse mode\n", channel, device_name);
        command_executed = 1;
    }
    
    // 设置连续波模式
    if (pwm_args.continuous->count)
    {
        pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_CONTINUOUS_WAVE_MODE, &pwm_data);
        printf("Set PWM channel %d on %s to continuous wave mode\n", channel, device_name);
        command_executed = 1;
    }
    
    // 关闭设备
    pwm_dev_close(dev_id);
    
    // 如果没有执行任何命令，显示帮助
    if (!command_executed)
    {
        show_pwm_help();
        return 1;
    }
    
    return 0;
}


// 打开风扇
void open_all_fans(void)
{
	int fan_dev = pwm_dev_open("fan", 0);
	if (fan_dev >= 0)
	{
		for (int i = 0; i < 4; i++)
		{
			pwm_dev_data_t pwm_data = {0};
			pwm_data.channel = i;
			pwm_dev_ioctl(fan_dev, PWM_DEV_IOCTL_DISABLE, &pwm_data);
			pwm_data.value = 999; // 设置周期�?ms
			pwm_dev_ioctl(fan_dev, PWM_DEV_IOCTL_SET_PERIOD, &pwm_data);
			pwm_data.value = 299; // 设置占空比为50%
			pwm_dev_ioctl(fan_dev, PWM_DEV_IOCTL_SET_DUTY, &pwm_data);
			pwm_dev_ioctl(fan_dev, PWM_DEV_IOCTL_CONTINUOUS_WAVE_MODE, &pwm_data);
			pwm_dev_ioctl(fan_dev, PWM_DEV_IOCTL_ENABLE, &pwm_data);
		}
	}
}

/**
 * @brief 注册网络接口命令
 */
void register_pwm_commands(void)
{
    pwm_args.info = arg_lit0("i", "info", "List PWM device information");
    pwm_args.device = arg_str0("d", "device", "<device>", "PWM device name (pwm_0, pwm_1)");
    pwm_args.channel = arg_int0("c", "channel", "<channel>", "Channel ID (0-15)");
    pwm_args.period = arg_int0("p", "period", "<period>", "PWM period (0-999999)");
    pwm_args.duty = arg_int0("y", "duty", "<duty>", "PWM duty cycle (0-999999)");
    pwm_args.enable = arg_lit0("e", "enable", "Enable PWM channel");
    pwm_args.disable = arg_lit0("b", "disable", "Disable PWM channel");
    pwm_args.one_pulse = arg_lit0("o", "one-pulse", "Set one pulse mode");
    pwm_args.continuous = arg_lit0("t", "continuous", "Set continuous wave mode");

    pwm_args.end = arg_end(9);
    const esp_console_cmd_t cmd = {
        .command = "pwm",
        .help = "pwm -i: List PWM device information\n"
                "pwm -d <device> -c <channel> [options]: Control PWM channel\n"
                "Options:\n"
                "  -p <period>    : Set PWM period\n"
                "  -y <duty>      : Set PWM duty cycle\n"
                "  -e             : Enable PWM channel\n"
                "  -b             : Disable PWM channel\n"
                "  -o             : Set one pulse mode\n"
                "  -t             : Set continuous wave mode\n",
        .hint = NULL,
        .func = &pwm_cmd_handler,
    };
    esp_console_cmd_register(&cmd);
}
