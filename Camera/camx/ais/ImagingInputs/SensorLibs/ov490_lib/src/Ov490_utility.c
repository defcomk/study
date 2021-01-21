/**
 * @file
 *          ov490_utility.c
 *
 * @brief
 *
 * @par Author (last changes):
 *          - Xin Huang
 *          - Phone +86 (0)21 6080 3824
 *          - xin.huang@continental-corporation.com
 *
 * @par Project Leader:
 *          - Pei Yu
 *          - Phone +86 (0)21 6080 3469
 *          - Yu.2.Pei@continental-corporation.com
 *
 * @par Responsible Architect:
 *          - Xin Huang
 *          - Phone +86 (0)21 6080 3824
 *          - xin.huang@continental-corporation.com
 *
 * @par Project:
 *          DiDi
 *
 * @par SW-Component:
 *          bridge driver header file
 *
 * @par SW-Package:
 *          libais_ov490.so
 *
 * @par SW-Module:
 *          AIS Camera HAL
 *
 * @par Description:
 *          Bridge driver  for DiDi board ov490 ISP  in AIS 
 * @note
 *
 * @par Module-History:
 *
 * @verbatim
 *
 *  Date            Author                  Reason
 *  18.03.2019      Xin Huang             Initial version.


 *
 * @endverbatim
 *
 * @par Copyright Notice:
 *
 * Copyright (c) Continental AG and subsidiaries 2015
 * Continental Automotive Holding (Shanghai)
 * Alle Rechte vorbehalten. All Rights Reserved.

 * The reproduction, transmission or use of this document or its contents is
 * not permitted without express written authority.
 * Offenders will be liable for damages. All rights, including rights created
 * by patent grant or registration of a utility model or design, are reserved.
 */

#include "Ov490_utility.h"

//#define AIS_LOG_TI_LIB_ERR(fmt...) AIS_LOG(AIS_MOD_ID_ADV7281_DRV, AIS_LOG_LVL_ERR, fmt)

/**************************************************************** 
 * gpio_export 
 ****************************************************************/  
int gpio_export(unsigned int gpio)  
{  
    int fd, len;  
    char buf[MAX_BUF];  
   
    fd = open("/sys/class/gpio/export", O_WRONLY);  
    if (fd < 0) {  
        //AIS_LOG_TI_LIB_ERR("gpio/export fd:%d", fd); 
       perror("gpio/export");  
        return fd;  
    }  
   
    len = snprintf(buf, sizeof(buf), "%d", gpio);  
    write(fd, buf, len);  
    close(fd);  
   
    return 0;  
}  
  
/**************************************************************** 
 * gpio_unexport 
 ****************************************************************/  
int gpio_unexport(unsigned int gpio)  
{  
    int fd, len;  
    char buf[MAX_BUF];  
   
    fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);  
    if (fd < 0) {  
        perror("gpio/export");  
        return fd;  
    }  
   
    len = snprintf(buf, sizeof(buf), "%d", gpio);  
    write(fd, buf, len);  
    close(fd);  
    return 0;  
}  
  
/**************************************************************** 
 * gpio_set_dir 
 ****************************************************************/  
int gpio_set_dir(unsigned int gpio, unsigned int out_flag)  
{  
    int fd, len;  
    char buf[MAX_BUF];  
   
    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", gpio);  
   
    fd = open(buf, O_WRONLY);  
    if (fd < 0) {  
        perror("gpio/direction");  
        // AIS_LOG_TI_LIB_ERR("gpio/direction");
       return fd;  
    }  
   
    if (out_flag)  
        write(fd, "out", 4);  
    else  
        write(fd, "in", 3);  
   
    close(fd);  
    return 0;  
}  
  
/**************************************************************** 
 * gpio_set_value 
 ****************************************************************/  
int gpio_set_value(unsigned int gpio, unsigned int value)  
{  
    int fd, len;  
    char buf[MAX_BUF];  
   
    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);  
   
    fd = open(buf, O_WRONLY);  
    if (fd < 0) {  
         //AIS_LOG_TI_LIB_ERR("gpio/set-value");
        perror("gpio/set-value");  
        SERR("/gpio%d: %s", gpio,strerror(errno));
        return fd;  
    }  
   
    if (value)  
        write(fd, "1", 2);  
    else  
        write(fd, "0", 2);  
   
    close(fd);  
    return 0;  
}  
  
/**************************************************************** 
 * gpio_get_value 
 ****************************************************************/  
int gpio_get_value(unsigned int gpio, unsigned int *value)  
{  
    int fd, len;  
    char buf[MAX_BUF];  
    char ch;  
  
    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);  
   
    fd = open(buf, O_RDONLY);  
    if (fd < 0) {  
        perror("gpio/get-value");  
        SERR("/gpio%d: %s", gpio,strerror(errno));
        return fd;  
    }  
   
    read(fd, &ch, 1);  
  
    if (ch != '0') {  
        *value = 1;  
    } else {  
        *value = 0;  
    }  
   
    close(fd);  
    return 0;  
}  
  
  
/**************************************************************** 
 * gpio_set_edge 
 ****************************************************************/  
  
int gpio_set_edge(unsigned int gpio, char *edge)  
{  
    int fd, len;  
    char buf[MAX_BUF];  
  
    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);  
   
    fd = open(buf, O_WRONLY);  
    if (fd < 0) {  
        perror("gpio/set-edge");  
        return fd;  
    }  
   
    write(fd, edge, strlen(edge) + 1);   
    close(fd);  
    return 0;  
}  
  
/**************************************************************** 
 * gpio_fd_open 
 ****************************************************************/  
  
int gpio_fd_open(unsigned int gpio)  
{  
    int fd, len;  
    char buf[MAX_BUF];  
  
    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);  
   
    fd = open(buf, O_RDONLY | O_NONBLOCK );  
    if (fd < 0) {  
        perror("gpio/fd_open");  
    }  
    return fd;  
}  
  
/**************************************************************** 
 * gpio_fd_close 
 ****************************************************************/  
  
int gpio_fd_close(int fd)  
{  
    return close(fd);  
}  



int test_get_time(unsigned long *pTime)
{
    struct timespec time;
    unsigned long msec;

    if (clock_gettime(CLOCK_MONOTONIC, &time) == -1)
    {
        //printf("Clock gettime failed\n");
        return 1;
    }
    msec = ((unsigned long)time.tv_sec * 1000) + (((unsigned long)time.tv_nsec / 1000) / 1000);
    *pTime = msec;

    return 0;
}

