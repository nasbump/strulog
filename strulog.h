#ifndef _H_STRULOG_
#define _H_STRULOG_

#include <stdint.h>

enum e_logger_level_t {
    LOGGER_TRACE  = 0,
    LOGGER_DEBUG  = 1,
    LOGGER_INFO   = 2,
    LOGGER_WARN   = 3,
    LOGGER_ERROR  = 4,
    LOGGER_NULL   = 5,
};

int32_t logger_init(enum e_logger_level_t lev, int32_t logsize, int32_t logcnt, char *logpath);
void logger_uninit(void);

/** logger for universal call **/
void logger_print(enum e_logger_level_t lev, const char *fmt, ...);

#define logger_trace(fmt, ...) logger_print(LOGGER_TRACE, "%s:%u " fmt "\n", __func__,__LINE__, ##__VA_ARGS__)
#define logger_debug(fmt, ...) logger_print(LOGGER_DEBUG, "%s:%u " fmt "\n", __func__,__LINE__, ##__VA_ARGS__)
#define logger_info(fmt,  ...) logger_print(LOGGER_INFO,  "%s:%u " fmt "\n", __func__,__LINE__, ##__VA_ARGS__)
#define logger_warn(fmt,  ...) logger_print(LOGGER_WARN,  "%s:%u " fmt "\n", __func__,__LINE__, ##__VA_ARGS__)
#define logger_error(fmt, ...) logger_print(LOGGER_ERROR, "%s:%u " fmt "\n", __func__,__LINE__, ##__VA_ARGS__)

/** logger for linked call **/
typedef struct s_strulog_t {
    struct s_strulog_t (*ptr)(char *, void *);
    struct s_strulog_t (*string)(char *, char *);
    struct s_strulog_t (*strsize)(char *, char *, int32_t);
    struct s_strulog_t (*int32)(char *, int32_t);
    struct s_strulog_t (*int64)(char *, int64_t);
    struct s_strulog_t (*uint32)(char *, uint32_t);
    struct s_strulog_t (*uint64)(char *, uint64_t);
    void (*post)(char *);
}strulog_t;

strulog_t strulog_new(enum e_logger_level_t lev, const char *f, int32_t line);
#define slogt  strulog_new(LOGGER_TRACE, __func__,__LINE__)
#define slogd  strulog_new(LOGGER_DEBUG, __func__,__LINE__)
#define slogi  strulog_new(LOGGER_INFO, __func__,__LINE__)
#define slogw  strulog_new(LOGGER_WARN, __func__,__LINE__)
#define sloge  strulog_new(LOGGER_ERROR, __func__,__LINE__)


#endif //_H_STRULOG_

