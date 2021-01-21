/**
 * @file
 *          ov490_utility.h
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

#ifndef __OV490_UTIL_H__
#define __OV490_UTIL_H__

#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <errno.h>  
#include <unistd.h>
#include <fcntl.h>  
#include <poll.h>
//#include "debug_lib.h"
#include <time.h>
#include "SensorDebug.h"

 /**************************************************************** 
 * Constants 
 ****************************************************************/  
   
#define SYSFS_GPIO_DIR "/sys/class/gpio"  
#define POLL_TIMEOUT (3 * 1000) /* 3 seconds */  
#define MAX_BUF 64  

#define DIR_OUT  (1)


int gpio_export(unsigned int gpio);
int gpio_unexport(unsigned int gpio);
int gpio_set_dir(unsigned int gpio, unsigned int out_flag);
int gpio_set_value(unsigned int gpio, unsigned int value);
int gpio_get_value(unsigned int gpio, unsigned int *value);
int gpio_set_edge(unsigned int gpio, char *edge);
int gpio_fd_open(unsigned int gpio);
int test_get_time(unsigned long *pTime);

#endif
