
#include <stdio.h>
#include "env.h"
#include "lfs.h"

// LFS文件系统实例，从lfs_port.c中引用
extern lfs_t lfs;

// 默认环境变量表，方便集中管理和修改
static const struct
{
    const char *key;
    const char *value;
} default_env_vars[] = DEFAULT_ENV_TABLE();

// 环境变量全局缓存，用于比较是否发生变化
static char **env_cache = NULL;
static int env_cache_count = 0;
static void update_env_cache(void);
/**
 * @brief 初始化环境变量模块
 * @return 0成功，负数失败
 */
int env_init(void)
{
    // 先设置默认环境变量
    int i;
    for (i = 0; i < sizeof(default_env_vars) / sizeof(default_env_vars[0]); i++)
    {
        setenv(default_env_vars[i].key, default_env_vars[i].value, 1);
    }

    // 再尝试从LFS加载环境变量，文件系统中的变量会覆盖默认值
    env_load();
    
    // 初始化缓存
    if (env_cache == NULL) {
        update_env_cache();
    }
    
    return 0;
}

/**
 * @brief 初始化环境变量迭代器
 * @param iter 迭代器指针
 */
void env_iter_init(env_iterator_t *iter)
{
    extern char **environ;
    iter->current = environ;
}

/**
 * @brief 获取下一个环境变量的键和值
 * @param iter 迭代器指针
 * @param key 输出参数，用于存储键名指针
 * @param key_len 输出参数，用于存储键名长度
 * @param value 输出参数，用于存储值指针
 * @param value_len 输出参数，用于存储值长度
 * @return 0表示成功获取到环境变量，-1表示已到达末尾
 * @note 此函数不会修改原始环境变量字符串
 */
int env_iter_next(env_iterator_t *iter, char **key, size_t *key_len, char **value, size_t *value_len)
{
    char *line = *iter->current;
    char *equals_pos;

    if (line == NULL)
    {
        return -1; // 已到达末尾
    }

    // 查找等号位置
    equals_pos = strchr(line, '=');
    if (equals_pos == NULL)
    {
        // 没有等号的情况
        *key = line;
        *key_len = strlen(line);
        *value = NULL;
        *value_len = 0;
    }
    else
    {
        // 计算键名长度
        *key_len = equals_pos - line;
        *key = line;

        // 计算值长度
        *value = equals_pos + 1;
        *value_len = strlen(*value);
    }

    // 移动迭代器到下一个位置
    iter->current++;

    return 0;
}

/**
 * @brief 获取下一个完整的环境变量字符串
 * @param iter 迭代器指针
 * @return 环境变量字符串，NULL表示已到达末尾
 */
char *env_iter_next_full(env_iterator_t *iter)
{
    char *line = *iter->current;

    if (line == NULL)
    {
        return NULL; // 已到达末尾
    }

    // 移动迭代器到下一个位置
    iter->current++;

    return line;
}

/**
 * @brief 释放环境变量缓存
 */
static void free_env_cache(void)
{
    if (env_cache != NULL) {
        for (int i = 0; i < env_cache_count; i++) {
            free(env_cache[i]);
        }
        vPortFree(env_cache);
        env_cache = NULL;
        env_cache_count = 0;
    }
}

/**
 * @brief 更新环境变量缓存
 */
static void update_env_cache(void)
{
    // 释放旧缓存
    free_env_cache();
    
    // 计算环境变量数量
    env_iterator_t iter;
    char *line;
    int count = 0;
    
    env_iter_init(&iter);
    while ((line = env_iter_next_full(&iter)) != NULL) {
        count++;
    }
    
    if (count == 0) {
        return;
    }
    
    // 分配新缓存
    env_cache = (char **)pvPortMalloc(sizeof(char *) * count);
    if (env_cache == NULL) {
        printf("env: failed to allocate cache\r\n");
        return;
    }
    
    // 填充缓存
    env_cache_count = count;
    count = 0;
    
    env_iter_init(&iter);
    while ((line = env_iter_next_full(&iter)) != NULL) {
        env_cache[count] = strdup(line);
        if (env_cache[count] == NULL) {
            printf("env: failed to duplicate cache entry\r\n");
            free_env_cache();
            return;
        }
        count++;
    }
}

/**
 * @brief 检查环境变量是否发生变化
 * @return 1表示发生变化，0表示未变化
 */
static int is_env_changed(void)
{
    if (env_cache == NULL) {
        return 1; // 缓存未初始化，认为发生变化
    }
    
    // 比较环境变量数量
    env_iterator_t iter;
    char *line;
    int count = 0;
    
    env_iter_init(&iter);
    while ((line = env_iter_next_full(&iter)) != NULL) {
        count++;
    }
    
    if (count != env_cache_count) {
        return 1; // 数量变化
    }
    
    // 比较每个环境变量
    int i = 0;
    env_iter_init(&iter);
    while ((line = env_iter_next_full(&iter)) != NULL) {
        if (strcmp(line, env_cache[i]) != 0) {
            return 1; // 内容变化
        }
        i++;
    }
    
    return 0; // 未发生变化
}

/**
 * @brief 保存环境变量到LFS文件系统
 * @return 0成功，负数失败
 */
int env_save(void)
{
    lfs_file_t file;
    int err;
    env_iterator_t iter;
    char *line;
    
    // 检查环境变量是否发生变化
    if (!is_env_changed()) {
        printf("env: variables unchanged, skipping save\r\n");
        return 0;
    }
    
    // 有变化，执行写入操作
    printf("env: variables changed, saving to LFS\r\n");
    
    // 打开文件，创建或截断
    err = lfs_file_open(&lfs, &file, ENV_FILE_PATH, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err)
    {
        printf("env: failed to open file for writing: %d\r\n", err);
        return err;
    }

    // 使用迭代器遍历环境变量表，写入所有环境变量
    env_iter_init(&iter);
    while ((line = env_iter_next_full(&iter)) != NULL)
    {
        lfs_ssize_t written = lfs_file_write(&lfs, &file, line, strlen(line));
        if (written < 0)
        {
            printf("env: failed to write variable: %d\r\n", (int)written);
            lfs_file_close(&lfs, &file);
            return written;
        }
        // 写入换行符
        written = lfs_file_write(&lfs, &file, "\n", 1);
        if (written < 0)
        {
            printf("env: failed to write newline: %d\r\n", (int)written);
            lfs_file_close(&lfs, &file);
            return written;
        }
    }

    // 关闭文件
    err = lfs_file_close(&lfs, &file);
    if (err)
    {
        printf("env: failed to close file: %d\r\n", err);
        return err;
    }

    // 更新环境变量缓存
    update_env_cache();
    
    printf("env: variables saved to LFS\r\n");
    return 0;
}

/**
 * @brief 从LFS文件系统加载环境变量
 * @return 0成功，负数失败
 */
int env_load(void)
{
    lfs_file_t file;
    int err;

    // 尝试打开环境变量文件
    err = lfs_file_open(&lfs, &file, ENV_FILE_PATH, LFS_O_RDONLY);
    if (err)
    {
        if (err == LFS_ERR_NOENT)
        {
            printf("env: file not found\r\n");
            return -1;
        }
        printf("env: failed to open file for reading: %d\r\n", err);
        return err;
    }

    // 读取文件内容
    char buffer[4096];
    char *line;
    char *saveptr1, *saveptr2;

    lfs_ssize_t read = lfs_file_read(&lfs, &file, buffer, sizeof(buffer) - 1);
    if (read < 0)
    {
        printf("env: failed to read file: %d\r\n", (int)read);
        lfs_file_close(&lfs, &file);
        return read;
    }

    buffer[read] = '\0'; // 确保字符串结束

    // 逐行解析环境变量
    for (line = strtok_r(buffer, "\n", &saveptr1); line != NULL; line = strtok_r(NULL, "\n", &saveptr1))
    {
        // 跳过空行和注释行
        if (*line == '\0' || *line == '#')
        {
            continue;
        }

        // 直接使用原始格式设置环境变量
        setenv(strtok_r(line, "=", &saveptr2), strtok_r(NULL, "=", &saveptr2), 1);
    }

    // 关闭文件
    err = lfs_file_close(&lfs, &file);
    if (err)
    {
        printf("env: failed to close file: %d\r\n", err);
        return err;
    }
    
    // 加载后更新缓存
    update_env_cache();

    printf("env: variables loaded from LFS\r\n");
    return 0;
}

/**
 * @brief 打印所有环境变量
 * @return 0成功，负数失败
 */
int env_print(void)
{
    env_iterator_t iter;
    char *line;
    int count = 0;

    // 使用迭代器遍历环境变量列表
    env_iter_init(&iter);
    while ((line = env_iter_next_full(&iter)) != NULL)
    {
        printf("%s\n", line);
        count++;
    }

    if (count == 0)
    {
        printf("No environment variables found\n");
        return 0;
    }

    printf("Total: %d environment variables\n", count);
    return 0;
}

/**
 * @brief 清除所有存储的环境变量
 * @return 0成功，负数失败
 */
int env_clean(void)
{
    int err;
    env_iterator_t iter;
    char *key_ptr;
    size_t key_len;
    char *value_ptr;
    size_t value_len;
    char key_buf[256]; // 临时缓冲区，用于存储null终止的键名

    // 使用迭代器遍历并清除所有环境变量
    env_iter_init(&iter);
    while (env_iter_next(&iter, &key_ptr, &key_len, &value_ptr, &value_len) == 0)
    {
        if (key_ptr != NULL && key_len > 0)
        {
            // 确保键名长度不超过缓冲区大小
            if (key_len >= sizeof(key_buf))
            {
                printf("env: variable key too long\r\n");
                continue;
            }
            
            // 复制键名到临时缓冲区并添加null终止符
            memcpy(key_buf, key_ptr, key_len);
            key_buf[key_len] = '\0';
            
            // 清除环境变量
            unsetenv(key_buf);
        }
    }

    // 删除LFS中的环境变量文件
    err = lfs_remove(&lfs, ENV_FILE_PATH);
    if (err && err != LFS_ERR_NOENT)
    {
        printf("env: failed to remove file: %d\r\n", err);
        return err;
    }
    
    // 清除后更新缓存
    update_env_cache();

    printf("env: variables cleaned\r\n");
    return 0;
}

/**
 * @brief 恢复默认环境变量
 * @return 0成功，负数失败
 */
int env_restore_default(void)
{

    int i;
    env_clean();

    // 重新设置默认环境变量
    for (i = 0; i < sizeof(default_env_vars) / sizeof(default_env_vars[0]); i++)
    {
        setenv(default_env_vars[i].key, default_env_vars[i].value, 1);
    }
    
    // 恢复后更新缓存
    update_env_cache();

    printf("env: default variables restored\r\n");
    return 0;
}
