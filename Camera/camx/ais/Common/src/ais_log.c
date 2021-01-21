/*
 * @file ais_log.c
 *
 * @brief defines all structures and functions which prints logs into memory/OS logging/file/console.
 *        it manages logs to be output to different destinations:
 *        memory, OS logging, file and console
 *
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#if defined(__QNXNTO__)
#include <process.h>
#include <sys/slog2.h>
#include <fcntl.h>
#elif defined(__INTEGRITY)
#include <sys/types.h>
#include <unistd.h>
#include <alltypes.h>
#include <util/boot_time_log_task.h>
#elif defined(__ANDROID__) || defined(__AGL__)
#include <sys/types.h>
#include <unistd.h>
#include <android/log.h>
#include <fcntl.h>
#elif defined(__LINUX)
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#endif

#if (defined (__GNUC__) && !defined(__INTEGRITY))
#define PUBLIC_API __attribute__ ((visibility ("default")))
#else
#define PUBLIC_API
#endif

#ifdef __INTEGRITY
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif
#define AIS_STDOUT stderr
#else
#define AIS_STDOUT stdout
#endif

#if defined(__INTEGRITY) || defined(__LINUX)
//#define AIS_GETTID() (unsigned int)syscall(224)
#define AIS_GETTID() (unsigned int)(uintptr_t)pthread_self()
#else
#define AIS_GETTID() (unsigned int)gettid()
#endif

#include "ais_log.h"

/**
 * delimiter in different OS
 */
#define AIS_LOG_DIR_DELM "/"

/**
 * OS logging ID
 */
#define AIS_LOG_OS_ID 415

/**
 * maximum length of file path
 */
#define AIS_LOG_PATH_SIZE 256

/**
 * maximum length of one line in the configuration file
 */
#define AIS_LOG_LINE_SIZE 256

/**
 * ais log mode
 * different mode means different destination
 */
#define AIS_LOG_MODE_MEM      (1 << 0)/**< output to memory */
#define AIS_LOG_MODE_OS       (1 << 1) /**< output to OS log */
#define AIS_LOG_MODE_FILE     (1 << 2) /**< output to a file */
#define AIS_LOG_MODE_CONSOLE  (1 << 3) /**< output to console */

#define AIS_LOG_MODE_MAX_NUM  4

/**
 * string for ais log level for printing
 */
PUBLIC_API const char *AIS_LOG_LVL_STR[AIS_LOG_LVL_MAX_NUM] =
    {"ALWAYS", "FATAL", "CRIT", "ERR", "WARN", "HIGH", "MED", "LOW", "DBG", "DBG1", "DBG2"};

/**
 * ais log configuration
 * different mode means different destination
 */
#define AIS_LOG_CONF_LVL_BMSK   (0x0FFF)
#define AIS_LOG_CONF_LVL_SHFT   0

#define AIS_LOG_CONF_MODE_BMSK  (0xF000)
#define AIS_LOG_CONF_MODE_SHFT  12


#define AIS_LOG_CONF_LVL(lvl) ((lvl) << AIS_LOG_CONF_LVL_SHFT)
#define AIS_LOG_CONF_MODE(mode) ((mode) << AIS_LOG_CONF_MODE_SHFT)

#define AIS_LOG_CONF_MAKE(mode, lvl) (AIS_LOG_CONF_MODE(mode) | AIS_LOG_CONF_LVL(lvl))

#define AIS_LOG_CONF_GET_LVL(x) (((x) & AIS_LOG_CONF_LVL_BMSK) >> AIS_LOG_CONF_LVL_SHFT)
#define AIS_LOG_CONF_GET_MODE(x) (((x) & AIS_LOG_CONF_MODE_BMSK) >> AIS_LOG_CONF_MODE_SHFT)

/**
 * ais default name
 */
#define AIS_LOG_DEFAULT_NAME "AIS-LOG"

/**
 * ais default configuration
 */
#if defined(__INTEGRITY)
#define AIS_LOG_DEFAULT_CONF AIS_LOG_CONF_MAKE(AIS_LOG_MODE_CONSOLE, AIS_LOG_LVL_WARN)
#elif defined(CAMERA_UNITTEST)
#define AIS_LOG_DEFAULT_CONF AIS_LOG_CONF_MAKE(AIS_LOG_MODE_CONSOLE, AIS_LOG_LVL_HIGH)
#else
#define AIS_LOG_DEFAULT_CONF AIS_LOG_CONF_MAKE(AIS_LOG_MODE_OS, AIS_LOG_LVL_HIGH)
#endif
/**
 * ais default size
 */
#define AIS_LOG_MEM_DEFAULT_SIZE (128*1024)
#define AIS_LOG_OS_DEFAULT_SIZE (128*1024)
#define AIS_LOG_FILE_DEFAULT_SIZE (128 * 1024 * 1024)

#define AIS_LOG_FILE_DEFAULT_PATH "."AIS_LOG_DIR_DELM"ais_log.txt" //the same folder of the program

/**
 * ais log configuration file naming
 * ais_log_XXX.conf, XXX is the program name
 * currently, it would be ais_server, qcarcam_test
 */
#define AIS_LOG_CONF_FILE_PREFIX "ais_log"
#define AIS_LOG_CONF_FILE_SUFFIX ".conf"

#define AIS_LOG_CONF_FILE_DEFAULT_NAME (AIS_LOG_CONF_FILE_PREFIX AIS_LOG_CONF_FILE_SUFFIX)

/**
 * ais log configuration default directory
 */
#if defined(__AGL__)
#define AIS_LOG_CONF_FILE_DIR "/data/misc/camera"
#else
#define AIS_LOG_CONF_FILE_DIR "." //the same folder of the program
#endif

/**
 * ais log item in config file
 */
#define AIS_LOG_ITEM_COMMENT          '#'

#define AIS_LOG_ITEM_MEM_SIZE         "MEM_SIZE"
#define AIS_LOG_ITEM_LEN_MEM_SIZE     8

#define AIS_LOG_ITEM_OS_SIZE          "OS_SIZE"
#define AIS_LOG_ITEM_LEN_OS_SIZE      7

#define AIS_LOG_ITEM_FILE_PATH        "FILE_PATH"
#define AIS_LOG_ITEM_LEN_FILE_PATH    9

#define AIS_LOG_ITEM_FILE_SIZE        "FILE_SIZE"
#define AIS_LOG_ITEM_LEN_FILE_SIZE    9

#define AIS_LOG_ITEM_GLOBAL           "GLOBAL"
#define AIS_LOG_ITEM_LEN_GLOBAL       6

#define AIS_LOG_ITEM_MODULE           "MODULE"
#define AIS_LOG_ITEM_LEN_MODULE       6

#define AIS_LOG_ITEM_MODE_MEM         'M'
#define AIS_LOG_ITEM_MODE_OS          'S'
#define AIS_LOG_ITEM_MODE_FILE        'F'
#define AIS_LOG_ITEM_MODE_CONSOLE     'C'


#define AIS_LOG_ALIGN_SIZE(x, y) (((x) + ((y)-1)) & ~((y)-1))

/**
 * log management structure
 */
typedef struct
{
    char name[32];                               /**< current program name */

    unsigned short conf;                         /**< global configuration */
    unsigned short mod_conf[AIS_MOD_ID_MAX_NUM]; /**< individual module configuration */

    long long start_time;                        /**< starting time of initialization in micro second */

    //memory logging
    char *p_mem_buf;                             /**< memory address */
    int  mem_size;                               /**< memory size */

    int mem_r_idx;
    int mem_w_idx;
    int mem_flag;

    char mem_buf[1024];

    //os logging
    int os_size;                                 /**< os log size */

    //file logging
    char file_path[AIS_LOG_PATH_SIZE];          /**< file path */
    int  file_size;                             /**< maximum file size */
    FILE *p_file_fd;                            /**< file handle */
    bool is_mod_log_file;

} s_ais_log_mgr;

static volatile bool sg_ais_log_initialized = false;
static pthread_mutex_t sgs_ais_log_mutex = PTHREAD_MUTEX_INITIALIZER;
static s_ais_log_mgr sgs_ais_log_mgr;
static int sg_fd_kpi = 0;

/**
 * initializes memory logging, allocates the buffer for logging and initializes read/write index
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_mem_init(s_ais_log_mgr *p)
{
    int rc = 0;
    if (p->mem_size <= 0)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    p->p_mem_buf = (char *)malloc(p->mem_size * sizeof(char *));
    if (p->p_mem_buf == NULL)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    p->mem_r_idx = 0;
    p->mem_w_idx = 0;
    p->mem_flag = 0;

EXIT_FLAG:

    return rc;
}

/**
 * uninitializes memory logging, frees the buffer for logging
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_mem_uninit(s_ais_log_mgr *p)
{
   if (p->p_mem_buf != NULL)
   {
        free(p->p_mem_buf);
        p->p_mem_buf = NULL;
   }

    return 0;
}

/**
 * prints formatted log into buffer
 * @param p points to a log management
 * @param level log level
 * @param fmt points to a format string
 * @param va variadic pointer
 * @return 0: success, others: failed
 */
int ais_log_mem_printf(s_ais_log_mgr *p, int level, const char *fmt, va_list va)
{
    int rc;
    struct timespec ts;
    long long curr;

#ifdef CAMERA_UNITTEST
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    curr = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 100000;
    curr -= p->start_time;

    rc = vsnprintf(p->mem_buf, sizeof(p->mem_buf), fmt, va);

    return rc;
}


#if defined(__QNXNTO__)

/**
 * initializes os logging and registers new buffer
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_os_init(s_ais_log_mgr *p)
{
    int rc;
    slog2_buffer_set_config_t config;
    slog2_buffer_t buf;

    config.buffer_set_name = "AIS";
    config.num_buffers = 1;
    config.verbosity_level = SLOG2_NOTICE;
    config.buffer_config[0].buffer_name = p->name;
    config.buffer_config[0].num_pages = AIS_LOG_ALIGN_SIZE(p->os_size, 4096) / 4096;

    rc = slog2_register(&config, &buf, SLOG2_DISCARD_NEWLINE);
    if (rc == 0)
    {
        slog2_set_default_buffer(buf);
    }

    return rc;
}

/**
 * uninitializes os logging
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_os_uninit(s_ais_log_mgr *p)
{
    slog2_reset();

    return 0;
}

/**
 * prints formatted log into buffer
 * @param p points to a log management
 * @param level log level
 * @param fmt points to a format string
 * @param va variadic pointer
 * @return 0: success, others: failed
 */
int ais_log_os_printf(s_ais_log_mgr *p, int level, const char *fmt, va_list va)
{
    vslog2f(NULL, AIS_LOG_OS_ID, level <= AIS_LOG_LVL_WARN ? SLOG2_ERROR : SLOG2_NOTICE, fmt, va);

    return 0;
}

#elif defined(__INTEGRITY)

/**
 * initializes os logging
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_os_init(s_ais_log_mgr *p)
{
    return 0;
}

/**
 * uninitializes os logging
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_os_uninit(s_ais_log_mgr *p)
{
    return 0;
}

int ais_log_os_printf(s_ais_log_mgr *p, int level, const char *fmt, va_list va)
{
    return 0;
}

#elif defined(__ANDROID__) || defined(__AGL__)

/**
 * initializes os logging
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_os_init(s_ais_log_mgr *p)
{
    return 0;
}

/**
 * uninitializes os logging
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_os_uninit(s_ais_log_mgr *p)
{
    return 0;
}

/**
 * prints formatted log into buffer
 * @param p points to a log management
 * @param level log level
 * @param fmt points to a format string
 * @param va variadic pointer
 * @return 0: success, others: failed
 */
int ais_log_os_printf(s_ais_log_mgr *p, int level, const char *fmt, va_list va)
{
    int rc;
    rc = __android_log_vprint(level <= AIS_LOG_LVL_WARN ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO,
                                p->name, fmt, va);
    return rc;
}

#elif defined(__LINUX)

/**
 * initializes os logging and opens logging for current process
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_os_init(s_ais_log_mgr *p)
{
    openlog(NULL, LOG_CONS | LOG_NDELAY, 0);
    //setlogmask(LOG_MASK(LOG_NOTICE));
    return 0;
}

/**
 * uninitializes os logging and closes logging
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_os_uninit(s_ais_log_mgr *p)
{
    closelog();
    return 0;
}

/**
 * prints formatted log into buffer
 * @param p points to a log management
 * @param level log level
 * @param fmt points to a format string
 * @param va variadic pointer
 * @return 0: success, others: failed
 */
int ais_log_os_printf(s_ais_log_mgr *p, int level, const char *fmt, va_list va)
{
    vsyslog(level <= AIS_LOG_LVL_WARN ? LOG_ERR : LOG_NOTICE, fmt, va);
    return 0;
}

#endif

/**
 * initializes file logging and opens a log file
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_file_init(s_ais_log_mgr *p)
{
    int rc = 0;

    p->p_file_fd = fopen(p->file_path, "a");
    if (p->p_file_fd == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

EXIT_FLAG:

    return rc;
}

/**
 * uninitializes os logging and closes the log file
 * @param p points to a log management
 * @return 0: success, others: failed
 */
int ais_log_file_uninit(s_ais_log_mgr *p)
{
    if (p->p_file_fd != NULL)
    {
        fclose(p->p_file_fd);
        p->p_file_fd = NULL;
    }
    return 0;
}

/**
 * prints formatted log into buffer
 * @param p points to a log management
 * @param fd points to file descriptor
 * @param level log level
 * @param fmt points to a format string
 * @param va variadic pointer
 * @return 0: success, others: failed
 */
int ais_log_fd_printf(s_ais_log_mgr *p, FILE *fp, int level, const char *fmt, va_list va)
{
    int rc;
    struct timespec ts;
    long long curr;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    curr = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#ifndef __AGL__
    curr -= p->start_time;
#endif

    fprintf(fp, "%03lld.%03lld [%u:%u] ", curr/1000, curr%1000,
            (unsigned int)getpid(), AIS_GETTID());
    rc = vfprintf(fp, fmt, va);
    if (level <= AIS_LOG_LVL_WARN)
    {
        fflush(fp);
    }

    return rc;
}

/**
 * prints formatted log into buffer
 * @param p points to a log management
 * @param id module id
 * @param level log level
 * @param fmt points to a format string
 * @param va variadic pointer
 * @return 0: success, others: failed
 */
int ais_log_va(s_ais_log_mgr *p, int id, int level, const char *fmt, va_list va)
{
    int rc = -1;
    va_list va_mem, va_os, va_file, va_console;

    unsigned int gmode = AIS_LOG_CONF_GET_MODE(p->conf);
    unsigned int mode = AIS_LOG_CONF_GET_MODE(p->mod_conf[id]);

    if ((gmode & AIS_LOG_MODE_MEM) || (mode & AIS_LOG_MODE_MEM))
    {
        va_copy(va_mem, va);
        rc = ais_log_mem_printf(p, level, fmt, va_mem);
    }

    if ((gmode & AIS_LOG_MODE_OS) || (mode & AIS_LOG_MODE_OS))
    {
        va_copy(va_os, va);
        rc = ais_log_os_printf(p, level, fmt, va_os);
    }

    if ((gmode & AIS_LOG_MODE_FILE) || (mode & AIS_LOG_MODE_FILE))
    {
        va_copy(va_file, va);
        rc = ais_log_fd_printf(p, p->p_file_fd, level, fmt, va_file);
    }

    if ((gmode & AIS_LOG_MODE_CONSOLE) || (mode & AIS_LOG_MODE_CONSOLE))
    {
        va_copy(va_console, va);
        rc = ais_log_fd_printf(p, AIS_STDOUT, level, fmt, va_console);
    }

    return rc;
}

/**
 * locks logging module
 * @return 0: success, others: failed
 */
int ais_log_lock(void)
{
    return pthread_mutex_lock(&sgs_ais_log_mutex);
}

/**
 * unlocks logging module
 * @return 0: success, others: failed
 */
int ais_log_unlock(void)
{
    return pthread_mutex_unlock(&sgs_ais_log_mutex);
}


/**
 * initializes all parameters to default values
 * @param p points to a log management
 * @return NONE
 */
static void ais_log_init_default(s_ais_log_mgr *p, char *p_name)
{
    int i;
    struct timespec ts;

    memset(p, 0, sizeof(s_ais_log_mgr));

    snprintf(p->name, sizeof(p->name), "%s", AIS_LOG_DEFAULT_NAME);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    p->start_time = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    p->mem_size = AIS_LOG_MEM_DEFAULT_SIZE;
    p->os_size = AIS_LOG_OS_DEFAULT_SIZE;

    if (p_name)
    {
        snprintf(p->file_path, sizeof(p->file_path), "%s%s%s", "/tmp/", p_name, ".log");
    }
    else
    {
        snprintf(p->file_path, sizeof(p->file_path), "%s", AIS_LOG_FILE_DEFAULT_PATH);
    }
    p->file_size = AIS_LOG_FILE_DEFAULT_SIZE;

    p->conf = AIS_LOG_DEFAULT_CONF;
    for (i = 0; i < AIS_MOD_ID_MAX_NUM; i++)
    {
        p->mod_conf[i] = AIS_LOG_DEFAULT_CONF;
    }

    //add your own default level and mode here

    return ;
}

/**
 * make full path of configuration file
 * @param p_path points to the path of configuration file
 *               NULL means the default path is used
 * @param p_name points to the name of current program
 *               NULL means the default name is used
 * @param p_conf points to the full path
 * @param size the length of the buffer pointed by p_conf
 * @return 0: success, others: failed
 */
static int ais_log_make_conf_path(char *p_path, char *p_name, char *p_conf, int size)
{
    if (p_path == NULL)
    {
        if (p_name == NULL)
        {
            snprintf(p_conf, size, "%s%s%s", AIS_LOG_CONF_FILE_DIR,
                        AIS_LOG_DIR_DELM, AIS_LOG_CONF_FILE_DEFAULT_NAME);
        }
        else
        {
            snprintf(p_conf, size, "%s%s%s_%s%s", AIS_LOG_CONF_FILE_DIR,
                        AIS_LOG_DIR_DELM, AIS_LOG_CONF_FILE_PREFIX,
                        p_name, AIS_LOG_CONF_FILE_SUFFIX);
        }
    }
    else
    {
        if (p_name == NULL)
        {
            snprintf(p_conf, size, "%s%s%s", p_path, AIS_LOG_DIR_DELM,
                    AIS_LOG_CONF_FILE_DEFAULT_NAME);
        }
        else
        {
            snprintf(p_conf, size, "%s%s%s_%s%s", p_path, AIS_LOG_DIR_DELM,
                    AIS_LOG_CONF_FILE_PREFIX, p_name, AIS_LOG_CONF_FILE_SUFFIX);
        }
    }

    return 0;
}

/**
 * parses a configuration file to set all parameters in log management
 * @param p_path points to the path of configuration file
 * @param p_name points to the name of current program
 * @return 0: success, others: failed
 */
int ais_log_parse(s_ais_log_mgr *p, char *p_path, char *p_name)
{
    int rc = 0;
    FILE *fp = NULL;
    char conf_path[AIS_LOG_PATH_SIZE] = "";
    int len;
    char *p_buf = NULL;
    char *p_head = NULL;
    char *p_tail = NULL;
    int i;
    int id;
    int level;
    char mode[AIS_LOG_MODE_MAX_NUM];
    int conf;
    int size;

    memset(conf_path, 0, sizeof(conf_path));

    ais_log_make_conf_path(p_path, p_name, conf_path, sizeof(conf_path));

//    printf("%s %p | %s %p | %s %s\n", __FUNCTION__,
//            p_path, p_path != NULL ? p_path : "",
//            p_name, p_name != NULL ? p_name : "",
//            conf_path);

    fp = fopen(conf_path, "rb");
    if (fp == NULL)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    //find the size of configuration file
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);

    //check if configuration file is too big
    if (size >= 1024 * 1024)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    //allocates a buffer to hold the whole configuration file
    p_buf = (char *)malloc((size + 1) * sizeof(char));
    if (p_buf == NULL)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    //read the whole configuration file
    fseek(fp, 0, SEEK_SET);
    len = fread(p_buf, 1, size, fp);
    if (len < size)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

    p_buf[size] = '\0';

    p_tail = p_buf;
    p_head = p_buf;
    while ((p_tail - p_buf) < len)
    {
        p_head = p_tail;

        //find line head and tail
        p_tail = strchr(p_head, '\n');
        if (p_tail != NULL)
        {
            *p_tail = '\0';
            p_tail++;
        }
        else
        {
            p_tail = p_buf + len;
        }

        //check if it is comment or blank line
        if (*p_head == AIS_LOG_ITEM_COMMENT
            || *p_head == '\r' || *p_head == '\n' || *p_head == '\0')
        {
            continue;
        }
        else
        {
            //remove appended comment in a line
            char *p_comm = strchr(p_head, AIS_LOG_ITEM_COMMENT);
            if (p_comm != NULL)
            {
                *p_comm = '\0';
            }

            if (strncmp(p_head, AIS_LOG_ITEM_MODULE, AIS_LOG_ITEM_LEN_MODULE) == 0)
            {
                id = 0;
                conf = 0;

                memset(mode, 0, sizeof(mode));
                sscanf(p_head, AIS_LOG_ITEM_MODULE "_%d=%d%c%c%c%c", &id, &level, mode, mode+1, mode+2, mode+3);

                for (i = 0; i < AIS_LOG_MODE_MAX_NUM; i++)
                {
                    switch ((char)toupper(mode[i]))
                    {
                    case AIS_LOG_ITEM_MODE_MEM:
                        conf |= AIS_LOG_MODE_MEM;
                        break;
                    case AIS_LOG_ITEM_MODE_OS:
                        conf |= AIS_LOG_MODE_OS;
                        break;
                    case AIS_LOG_ITEM_MODE_FILE:
                        conf |= AIS_LOG_MODE_FILE;
                        break;
                    case AIS_LOG_ITEM_MODE_CONSOLE:
                        conf |= AIS_LOG_MODE_CONSOLE;
                        break;
                    }
                }

                if (id < AIS_MOD_ID_MAX_NUM)
                {
                    p->mod_conf[id] = AIS_LOG_CONF_MAKE(conf, level);
                }
            }
            else if (strncmp(p_head, AIS_LOG_ITEM_GLOBAL, AIS_LOG_ITEM_LEN_GLOBAL) == 0)
            {
                conf = 0;

                memset(mode, 0, sizeof(mode));
                sscanf(p_head, AIS_LOG_ITEM_GLOBAL "=%d%c%c%c%c", &level, mode, mode+1, mode+2, mode+3);

                for (i = 0; i < AIS_LOG_MODE_MAX_NUM; i++)
                {
                    switch ((char)toupper(mode[i]))
                    {
                    case AIS_LOG_ITEM_MODE_MEM:
                        conf |= AIS_LOG_MODE_MEM;
                        break;
                    case AIS_LOG_ITEM_MODE_OS:
                        conf |= AIS_LOG_MODE_OS;
                        break;
                    case AIS_LOG_ITEM_MODE_FILE:
                        conf |= AIS_LOG_MODE_FILE;
                        break;
                    case AIS_LOG_ITEM_MODE_CONSOLE:
                        conf |= AIS_LOG_MODE_CONSOLE;
                        break;
                    }
                }

                p->conf = AIS_LOG_CONF_MAKE(conf, level);
            }
            else if (strncmp(p_head, AIS_LOG_ITEM_MEM_SIZE, AIS_LOG_ITEM_LEN_MEM_SIZE) == 0)
            {
                size = 0;
                sscanf(p_head, AIS_LOG_ITEM_MEM_SIZE "=%d", &size);
                if (size > 0)
                {
                    p->mem_size = size;
                }
            }
            else if (strncmp(p_head, AIS_LOG_ITEM_OS_SIZE, AIS_LOG_ITEM_LEN_OS_SIZE) == 0)
            {
                size = 0;
                sscanf(p_head, AIS_LOG_ITEM_OS_SIZE "=%d", &size);
                if (size > 0)
                {
                    p->os_size = size;
                }
            }
            else if (strncmp(p_head, AIS_LOG_ITEM_FILE_PATH, AIS_LOG_ITEM_LEN_FILE_PATH) == 0)
            {
                snprintf(p->file_path, sizeof(p->file_path), "%s", p_head + AIS_LOG_ITEM_LEN_FILE_PATH + 1);
            }
            else if (strncmp(p_head, AIS_LOG_ITEM_FILE_SIZE, AIS_LOG_ITEM_LEN_FILE_SIZE) == 0)
            {
                size = 0;
                sscanf(p_head, AIS_LOG_ITEM_FILE_SIZE "=%d", &size);
                if (size > 0)
                {
                    p->file_size = size;
                }
            }
        }
    }

    for (i = 0; i < AIS_MOD_ID_MAX_NUM; i++)
    {
        unsigned int m_mode;

        m_mode = AIS_LOG_CONF_GET_MODE(p->mod_conf[i]);
        if (m_mode & AIS_LOG_MODE_FILE)
        {
            p->is_mod_log_file = true;
        }
    }

EXIT_FLAG:

    if (p_buf != NULL)
    {
        free(p_buf);
        p_buf = NULL;
    }

    if (fp != NULL)
    {
        fclose(fp);
        fp = NULL;
    }

    return rc;
}

/**
 * initializes log management, parses configuration file and allocates all resources
 * @param p_path points to the path of configuration file
 *               NULL means the default path is used
 * @param p_name points to the name of current program,
 *               which is used to compose the configuration file and os logging name
 *               NULL means the default name is used
 * @return 0: success, others: failed
 */
PUBLIC_API int ais_log_init(char *p_path, char *p_name)
{
    int rc = 0;
    s_ais_log_mgr *p = &sgs_ais_log_mgr;
    unsigned int gmode;

    ais_log_lock();

    if (sg_ais_log_initialized)
    {
        goto EXIT_FLAG;
    }

    ais_log_init_default(p, p_name);

    if (p_name != NULL)
    {
        snprintf(p->name, sizeof(p->name), "%s", p_name);
    }

    ais_log_parse(p, p_path, p_name);

    //remove memory logging which is not supported currently
    p->conf &= ~AIS_LOG_CONF_MODE(AIS_LOG_MODE_MEM);

    gmode = AIS_LOG_CONF_GET_MODE(p->conf);

    if (gmode & AIS_LOG_MODE_MEM)
    {
        rc = ais_log_mem_init(p);
        if (rc != 0)
        {
            p->conf &= ~AIS_LOG_CONF_MODE(AIS_LOG_MODE_MEM);
        }
    }

    if (gmode & AIS_LOG_MODE_OS)
    {
        rc = ais_log_os_init(p);
        if (rc != 0)
        {
            p->conf &= ~AIS_LOG_CONF_MODE(AIS_LOG_MODE_OS);
        }
    }

    if ((gmode & AIS_LOG_MODE_FILE) || p->is_mod_log_file)
    {
        rc = ais_log_file_init(p);
        if (rc != 0)
        {
            int i;

            p->conf &= ~AIS_LOG_CONF_MODE(AIS_LOG_MODE_FILE);
            // Remove AIS_LOG_MODE_FILE flag from all modules
            for (i = 0; i < AIS_MOD_ID_MAX_NUM; i++)
            {
                p->mod_conf[i] &= ~AIS_LOG_CONF_MODE(AIS_LOG_MODE_FILE);
            }
        }
    }

#if defined(__QNXNTO__)
    sg_fd_kpi = open("/dev/bmetrics", O_WRONLY);
#elif defined(__ANDROID__)
    sg_fd_kpi = open("/sys/kernel/debug/bootkpi/kpi_values", O_RDWR);
#endif

    sg_ais_log_initialized = true;

EXIT_FLAG:

    ais_log_unlock();

    return rc;
}

/**
 * initializes log management and frees all resources
 * @return 0: success, others: failed
 */
PUBLIC_API int ais_log_uninit(void)
{
    unsigned int gmode;

    ais_log_lock();

    if (!sg_ais_log_initialized)
    {
        goto EXIT_FLAG;
    }

    gmode = AIS_LOG_CONF_GET_MODE(sgs_ais_log_mgr.conf);

    sg_ais_log_initialized = false;

    if (gmode & AIS_LOG_MODE_MEM)
    {
        ais_log_mem_uninit(&sgs_ais_log_mgr);
    }

    if (gmode & AIS_LOG_MODE_OS)
    {
        ais_log_os_uninit(&sgs_ais_log_mgr);
    }

    if (gmode & AIS_LOG_MODE_FILE)
    {
        ais_log_file_uninit(&sgs_ais_log_mgr);
    }

    if (sg_fd_kpi > 0)
    {
        close(sg_fd_kpi);
        sg_fd_kpi = 0;
    }

EXIT_FLAG:

    ais_log_unlock();

    return 0;
}

/**
 * checks if one message can be printed out
 * @param id module id
 * @param level message level
 * @return 0: success, others: failed
 */
static inline int ais_log_check_level(int id, int level)
{
    int rc = 0;

    if (id < 0 || id >= AIS_MOD_ID_MAX_NUM)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    if (!sg_ais_log_initialized)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if (AIS_LOG_CONF_GET_LVL(sgs_ais_log_mgr.conf) < level
        && AIS_LOG_CONF_GET_LVL(sgs_ais_log_mgr.mod_conf[id]) < level)
    {
        rc = -3;
        goto EXIT_FLAG;
    }

EXIT_FLAG:

    return rc;
}

/**
 * sets level if one message can be printed out
 * @param id module id
 * @param level message level
 * @return 0: success, others: failed
 */
PUBLIC_API int ais_log_set_level(int id, int level)
{
    int rc = 0;

    if (id < 0 || id > AIS_MOD_ID_MAX_NUM)
    {
        rc = -1;
        goto EXIT_FLAG;
    }

    if (!sg_ais_log_initialized)
    {
        rc = -2;
        goto EXIT_FLAG;
    }

    if( AIS_MOD_ID_MAX_NUM == id )
    {
        sgs_ais_log_mgr.conf = AIS_LOG_CONF_MAKE(AIS_LOG_CONF_GET_MODE(sgs_ais_log_mgr.conf), level);
    }
    else
    {
        sgs_ais_log_mgr.mod_conf[id] = AIS_LOG_CONF_MAKE(AIS_LOG_CONF_GET_MODE(sgs_ais_log_mgr.mod_conf[id]), level);
    }

EXIT_FLAG:

    return rc;
}


/**
 * prints formatted log into buffer and is not thread-safe
 * generally is used in signal handlers when log mutex may be locked outside
 * @param id module id
 * @param level log level
 * @param fmt points to a format string
 * @return >0: success, others: failed
 */
PUBLIC_API int ais_log_unsafe(int id, int level, const char *fmt, ...)
{
    int rc;

    rc = ais_log_check_level(id, level);
    if (rc == 0)
    {
        va_list va;
        va_start(va, fmt);

        rc = ais_log_va(&sgs_ais_log_mgr, id, level, fmt, va);

        va_end(va);
    }

    return rc;
}

/**
 * prints formatted log into buffer and is thread-safe
 * @param id module id
 * @param level log level
 * @param fmt points to a format string
 * @return >0: success, others: failed
 */
PUBLIC_API int ais_log(int id, int level, const char *fmt, ...)
{
    int rc;

    //check level first to avoid locking everywhere.
    //because log level won't change after initialzation,
    //even if it changes, it won't affect the program behavoir,
    //only some messages are not printed out.
    rc = ais_log_check_level(id, level);
    if (rc == 0)
    {
        va_list va;
        va_start(va, fmt);
        ais_log_lock();

        rc = ais_log_va(&sgs_ais_log_mgr, id, level, fmt, va);

        ais_log_unlock();
        va_end(va);
    }

    return rc;
}

/**
 * flushes all logs into buffer and is not thread-safe
 * generally is used in signal handlers when log mutex may be locked outside
 * @return 0: success, others: failed
 */
PUBLIC_API int ais_log_flush_unsafe(void)
{
    unsigned int gmode = AIS_LOG_CONF_GET_MODE(sgs_ais_log_mgr.conf);

    if (gmode & AIS_LOG_MODE_MEM)
    {

    }

    if (gmode & AIS_LOG_MODE_OS)
    {

    }

    if (gmode & AIS_LOG_MODE_FILE)
    {
        fflush(sgs_ais_log_mgr.p_file_fd);
    }

    if (gmode & AIS_LOG_MODE_CONSOLE)
    {
        fflush(stdout);
    }

    return 0;
}

/**
 * flushes all logs into buffer and is thread-safe
 * @return 0: success, others: failed
 */
PUBLIC_API int ais_log_flush(void)
{
    ais_log_lock();

    ais_log_flush_unsafe();

    ais_log_unlock();

    return 0;
}

#if defined(__QNXNTO__)
static char sg_kpi_msg[AIS_EVENT_KPI_MAX][64] =
{
    "bootmarker DRIVER Camera Init",
    "bootmarker DRIVER Camera Ready",
    "bootmarker BE SERVER Init",
    "bootmarker BE SERVER Ready",
    "bootmarker /dev/qcarcam Camera Start",
    "bootmarker /dev/qcarcam Camera First Frame",
    "bootmarker DRIVER Camera Sensor prog start",
    "bootmarker DRIVER Camera Sensor prog end"
};
#elif defined(__INTEGRITY)
static char sg_kpi_msg[AIS_EVENT_KPI_MAX][64] =
{
    "M - CAMERA DRIVER Init",
    "M - CAMERA DRIVER Ready",
    "M - CAMERA BE SERVER Init",
    "M - CAMERA BE SERVER Ready",
    "M - Camera Start",
    "M - Camera First Frame",
    "M - Camera Sensor prog start",
    "M - Camera Sensor prog end"
};
#else
static char sg_kpi_msg[AIS_EVENT_KPI_MAX][64] =
{
    "DRIVER Camera Init",
    "DRIVER Camera Ready",
    "DRIVER BE Camera Init",
    "DRIVER BE Camera Ready",
    "M - Camera Start",
    "M - Camera First Frame",
    "DRIVER Camera Sensor prog start",
    "DRIVER Camera Sensor prog end"
};
#endif

PUBLIC_API void ais_log_kpi(ais_event_kpi_t event)
{
#if defined(__INTEGRITY)
    if (event < AIS_EVENT_KPI_MAX)
    {
        char* msg = sg_kpi_msg[event];
        BootTimeLog_Task_Checkpoint(msg);
    }
#else
    if (sg_fd_kpi && event < AIS_EVENT_KPI_MAX)
    {
        char* msg = sg_kpi_msg[event];
        (void)write(sg_fd_kpi, msg, strlen(msg)+1);
    }
#endif
}
