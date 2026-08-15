/**
 * @file esp.c
 * @brief ESP TCP连接巴法云
 * @version 0.1
 * @date 2025-12-09
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "esp.h"
#include <string.h>
#include <stdio.h>

uint8_t ESP_buffer[ESP_RX_BUF_SIZE];
volatile int32_t ESP_rx_len; /* ISR 回调与主路径共享，须 volatile */
// 接收中断回调函数,在HAL_UARTEx_RxEventCallback中调用
void ESP_UART_Callback(uint16_t Size)
{
    ESP_rx_len = (int32_t)Size;                                                           // 串口接收中断
    if (HAL_UARTEx_ReceiveToIdle_IT(ESP_UART_HANDLE, ESP_buffer, sizeof(ESP_buffer)) != HAL_OK)
    {
        /* 重挂失败（HAL_BUSY 等）：接收通道停摆，ESP_Get_Receive_Data 会自愈重挂 */
        ESP_rx_len = 0;
    }
}
// 启动接收中断
void ESP_UART_Receive_Start(void)
{
    ESP_rx_len = 0;
    memset(ESP_buffer, 0, sizeof(ESP_buffer));
    (void)HAL_UARTEx_ReceiveToIdle_IT(ESP_UART_HANDLE, ESP_buffer, sizeof(ESP_buffer));
}

/* 接收通道自愈：RxState 非 BUSY_RX 说明中断接收已停，重新挂载 */
static void ESP_UART_ReArmIfNeeded(void)
{
    if ((ESP_UART_HANDLE)->RxState != HAL_UART_STATE_BUSY_RX)
    {
        ESP_rx_len = 0;
        (void)HAL_UARTEx_ReceiveToIdle_IT(ESP_UART_HANDLE, ESP_buffer,
                                          sizeof(ESP_buffer));
    }
}

HAL_StatusTypeDef ESP_Send_data_len(const uint8_t *data, uint16_t len, uint16_t timeout)
{
    ESP_rx_len = 0;
    memset(ESP_buffer, 0, ESP_RX_BUF_SIZE);
    ESP_UART_ReArmIfNeeded();
    return HAL_UART_Transmit(ESP_UART_HANDLE, data, len, timeout); // 数据发送
}

/**
 * @brief 获取接收数据
 *
 * @param recv_buf 接收数据缓冲区
 * @param recv_len 接收数据长度
 * @param timeout 超时时间
 * @return int32_t 成功返回剩余时间，失败返回-1
 */
int32_t ESP_Get_Receive_Data(uint8_t *recv_buf, int32_t *recv_len, uint32_t timeout)
{
    uint32_t start_tick = HAL_GetTick(); // 记录计时起始时间戳
    uint32_t elapsed_time = 0;           // 计算实际消耗时间,
    uint32_t remaining_timeout = 0;      // 剩余时间

    ESP_UART_ReArmIfNeeded();
    while (ESP_rx_len <= 0)
    {
        ESP_UART_ReArmIfNeeded(); /* 等待期间持续自愈 */
        if ((HAL_GetTick() - start_tick) >= timeout)
        {
            break;
        }
        HAL_Delay(1);
    }
    if (ESP_rx_len > 0)
    {
        elapsed_time = HAL_GetTick() - start_tick;
        remaining_timeout = (timeout >= elapsed_time) ? (timeout - elapsed_time) : 0;
        int32_t rx_len = ESP_rx_len; /* volatile 共享值单次采样，避免回调间两次读不一致 */
        *recv_len = (rx_len < *recv_len) ? rx_len : *recv_len; /* 取接收长度与容量较小者 */
        memcpy(recv_buf, ESP_buffer, (size_t)*recv_len);
        ESP_rx_len = 0; // 清空接收缓冲,继续接收
        return remaining_timeout;
    }
    else
    {
        return -1;
    }
}

/**
 * @brief 发送指令，并指定时间内接收指定的数据
 *
 * @param cmd  AT指令注意带\r\n
 * @param wait_string   等待字符串
 * @param timeout  超时时间ms
 * @return int8_t 0:成功 -1:失败
 */
int8_t ESP_Send_AT_Cmd(const char *cmd, const char *wait_string, uint32_t timeout)
{
    int32_t remaining_timeout = (int32_t)timeout;
    uint8_t recv_buf[ESP_RX_BUF_SIZE + 1]; /* 多留 1 字节以便安全添加 '\0' */
    ESP_Send_data_len((const uint8_t *)cmd, strlen(cmd), 1000); // 发送命令

    while (1)
    {
        int32_t recv_len = (int32_t)sizeof(recv_buf); /* 每轮重新传入可用容量 */
        remaining_timeout = ESP_Get_Receive_Data(recv_buf, &recv_len, (uint32_t)remaining_timeout);
        if (remaining_timeout < 0)
        {
            break;
        }
        if (recv_len > 0)
        {
            recv_buf[recv_len] = '\0'; /* 对本次复制出的快照安全终止后再匹配 */
            if (strstr((const char *)recv_buf, wait_string))
            {
                return 0; // 成功
            }
        }
        if (remaining_timeout == 0)
        {
            break;
        }
    }
    return -1;
}

// 退出透传模式：+++ 不带 \r\n，不依赖 "OK" 应答，发送后延时等待模块退出。
// 期间收到的 "OK" 会在下一次 ESP_Send_data_len 发送前被清空，不影响后续 AT 应答匹配。
int8_t ESP_Exit_Transmit_Mode(void)
{
    ESP_Send_data_len((const uint8_t *)"+++", 3U, 1000U);
    HAL_Delay(1000U);
    printf("ESP Exit Transmit Mode , Done\r\n");
    return 0;
}

int8_t ESP_Connect_WiFi(char *ssid, char *password)
{
    uint8_t i = 0;
    char cmd[50];             // 指令缓冲
    ESP_Exit_Transmit_Mode(); // 退出透传模式

    while (ESP_Send_AT_Cmd("AT\r\n", "OK", 500)) // 测试模块状态
    {
        i++;
        if (i >= 3)
        {
            printf("ESP Send cmd: AT , Error\r\n");
            return -1;
        }
    }
    if (ESP_Send_AT_Cmd("ATE0\r\n", "OK", 500)) // 关闭回显
    {
        printf("ESP Send cmd: ATE0 , Error\r\n");
        return -1;
    }

    if (ESP_Send_AT_Cmd("AT+CWMODE=3\r\n", "OK", 500)) // 混合wifi模式 (去连接wifi路由器)
    {
        printf("ESP Send cmd: AT+CWMODE=3 , Error\r\n");
        return -1;
    }

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password); // 拼接指令
    if (ESP_Send_AT_Cmd(cmd, "OK", 10000))                                    // 连接WiFi
    {
        printf("ESP Send cmd: AT+CWJAP, Error\r\n"); // 不回显凭据
        return -1;
    }
    return 0;
}

// 连接服务器bemfa.com，TCP端口8344, MQTT端口：9501，连接成功后，进入透传模式
int8_t ESP_Connect_Server(char *ip, char *port)
{
    char cmd[50];                                        // 指令缓冲
    if (ESP_Send_AT_Cmd("AT+CIPMODE=1\r\n", "OK", 2000)) // 设置透传模式
    {
        printf("ESP Send cmd: AT+CIPMODE=1 , Error\r\n");
        return -1;
    }
    // 连接服务器和端口AT+CIPSTART="TCP","bemfa.com",8344
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%s\r\n", ip, port);
    if (ESP_Send_AT_Cmd(cmd, "OK", 5000)) // 连接服务器
    {
        printf("ESP Send cmd: %s, Error\r\n", cmd);
        return -1;
    }
    // 进入透传模式，后面发的都会无条件传输
    if (ESP_Send_AT_Cmd("AT+CIPSEND\r\n", "OK", 3000)) // 进入透传模式
    {
        printf("ESP Send cmd: AT+CIPSEND\r\n, Error\r\n");
        return -1;
    }
    return 0;
}

// ESP连接WiFi并连接服务器
int8_t ESP_Init(void)
{
    HAL_Delay(2000); // 等待ESP开机
    ESP_UART_Receive_Start();
    printf("ESP Start, WIFI connecting...\r\n");
    if (ESP_Connect_WiFi(WIFI_SSID, WIFI_PASSWORD)) // 连接WiFi
    {
        printf("ESP Connect WiFi Error\r\n");
        return -1;
    }
    printf("ESP Connect WiFi Success\r\n");

    if (ESP_Connect_Server(TCP_SERVER, TCP_SERVER_PORT)) // 连接服务器
    {
        printf("ESP Connect Server Error\r\n");
        return -1;
    }
    printf("ESP Connect Server Success\r\n");
    return 0;
}
