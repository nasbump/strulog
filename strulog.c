#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>

#ifdef _WIN32
	#include <windows.h>
	#include <io.h>
	#include <process.h>
	#include <share.h>
	#define F_OK  0
	#define access(p,m)   _access(p, m)
	#define write(fd,b,s) _write(fd,b,s)
	#define lseek(fd,o,w) _lseek(fd,o,w)
	#define close(fd)     _close(fd)
	#define ts2localtime(ts, lt)   localtime_s(lt, ts)
#else
	#include <unistd.h>
	#define ts2localtime(ts, lt)   localtime_r(ts, lt)
#endif
#include <time.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "strulog.h"

#ifndef NULL
  #define NULL ((void *)0)
#endif

#define MAX_LOGPATH_SIZE       (512)
#define LOGGER_LINE_SIZE       (4<<10)

#ifdef _WIN32
  #define LOG_FILE_OPEN(fd, path) _sopen_s(&fd, path, _O_BINARY|_O_CREAT|_O_WRONLY, _SH_DENYNO, _S_IWRITE)
  #define LOG_FILE_REOPEN(fd, path) _sopen_s(&fd, path, _O_BINARY|_O_CREAT|_O_WRONLY|_O_TRUNC, _SH_DENYNO, _S_IWRITE)
#else
  #ifdef HAVE_NO_CLOEXEC
    #define LOG_FILE_OPEN(fd, path) fd=open(path, O_CREAT|O_WRONLY, 0666)
    #define LOG_FILE_REOPEN(fd, path) fd=open(path, O_CREAT|O_WRONLY|O_TRUNC, 0666)
  #else
    #define LOG_FILE_OPEN(fd, path)   fd=open(path, O_CREAT|O_WRONLY|O_CLOEXEC, 0666)
    #define LOG_FILE_REOPEN(fd, path) fd=open(path, O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC, 0666)
  #endif
#endif // _WIN32

#define LOG_FD_CLOSE(fd) if(fd >= 0) { close(fd); (fd)=-1; }


/* default logger config */
struct s_logger_cfg_t {
    char curlogpath[MAX_LOGPATH_SIZE];
    int32_t pathlen;
    int32_t logfd;
    int32_t maxlogsize;
    int32_t maxlogcnt;
    int32_t curlogsize;
} s_logger_cfg;

static enum e_logger_level_t s_loglev = LOGGER_INFO;

static char *loglev_text = "TDIWE";
static char buf[LOGGER_LINE_SIZE];
static int32_t bsize;
static int32_t s_curpid;

static strulog_t s_strulog;
static strulog_t ptr(char *key, void *val);
static strulog_t string(char *key, char *val);
static strulog_t strsize(char *key, char *val, int32_t vsize);
static strulog_t hex(char *key, void *val, int32_t vsize);
static strulog_t int16(char *key, int16_t val);
static strulog_t int32(char *key, int32_t val);
static strulog_t int64(char *key, int64_t val);
static strulog_t uint16(char *key, uint16_t val);
static strulog_t uint32(char *key, uint32_t val);
static strulog_t uint64(char *key, uint64_t val);
static void post(char *msg);

int32_t logger_init(enum e_logger_level_t level, int32_t logsize, int32_t logcnt, char *logpath){
    s_loglev = level <= LOGGER_NULL && level >= LOGGER_TRACE ? level : LOGGER_INFO;
    s_logger_cfg.maxlogsize = logsize > 0 ? logsize : (5<<20);
    s_logger_cfg.maxlogcnt  = logcnt  > 0 ? logcnt  : (1);
    s_logger_cfg.logfd = -1;

    s_strulog.string = string;
    s_strulog.strsize = strsize;
    s_strulog.hex = hex;
    s_strulog.int16 = int16;
    s_strulog.int32 = int32;
    s_strulog.int64 = int64;
    s_strulog.uint16 = uint16;
    s_strulog.uint32 = uint32;
    s_strulog.uint64 = uint64;
    s_strulog.post = post;

    s_curpid = (uint32_t)getpid();

    if(!logpath || !*logpath) {
        s_logger_cfg.curlogpath[0] = 0;
        s_logger_cfg.pathlen = 0;
        return 0;
    }

    LOG_FILE_OPEN(s_logger_cfg.logfd, logpath);
    if(s_logger_cfg.logfd < 0) {
        fprintf(stderr, "error!open log file fail!ret=%d,error:%s\n", errno, strerror(errno));
        s_logger_cfg.pathlen = 0;
        return 0;
    }

    s_logger_cfg.curlogsize = lseek(s_logger_cfg.logfd, 0, SEEK_END);
    s_logger_cfg.pathlen = snprintf(s_logger_cfg.curlogpath, sizeof(s_logger_cfg.curlogpath), "%s", logpath);

    return 0;
}

void logger_uninit(void){
    LOG_FD_CLOSE(s_logger_cfg.logfd);
    s_logger_cfg.curlogpath[0] = 0;
    s_logger_cfg.pathlen = 0;
}
#ifdef _WIN32
static void slog_curtime(uint32_t *sec, uint32_t *msec) {
    SYSTEMTIME lt;
    memset(&lt, 0, sizeof(SYSTEMTIME));
    GetLocalTime(&lt);
    
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    tm.tm_sec = lt.wSecond;
    tm.tm_min = lt.wMinute;
    tm.tm_hour = lt.wHour;
    tm.tm_mday = lt.wDay;
    tm.tm_mon = lt.wMonth - 1;
    tm.tm_year = lt.wYear - 1900;
    
    *sec = (uint32_t)mktime(&tm);
    *msec = (uint32_t)lt.wMilliseconds;
}
#else
static void slog_curtime(uint32_t *sec, uint32_t *msec) {
    struct timespec tp = {0};
    clock_gettime(CLOCK_REALTIME, &tp);
    *sec = (uint32_t)tp.tv_sec;
    *msec = (uint32_t)(tp.tv_nsec / 1000000);
}
#endif

static void _logger_write(int32_t fd, char *pbuf, int32_t len) {
    char *p = pbuf;
    int32_t wlen = 0, left = len;

    while(left > 0) {
        wlen = write(fd, p, left);
        if (wlen < 0) {
            if(errno == EINTR) {
                continue;
            } else {
                return;
            }
        } else {
            left -= wlen;
            p += wlen;
        }
    }
}

static void logger_rotate_iter(int32_t curidx) {
    char newpath[MAX_LOGPATH_SIZE];

    snprintf(newpath, sizeof(newpath), "%s.%d", s_logger_cfg.curlogpath, curidx+1);
    if(access(newpath, F_OK)) { // existed
        if(curidx + 1 < s_logger_cfg.maxlogcnt) {
            logger_rotate_iter(curidx + 1);
        } else {
            remove(newpath);
        }
    }

    if(curidx) {
        snprintf(s_logger_cfg.curlogpath + s_logger_cfg.pathlen, 8, "%s.%d", s_logger_cfg.curlogpath, curidx);
    }
    int32_t ret = rename(s_logger_cfg.curlogpath, newpath);
    if(ret != 0) {
        fprintf(stderr, "error!rename [%s] to [%s] failed!ret=%d,%s\n",
                s_logger_cfg.curlogpath, newpath, errno, strerror(errno));
    }
}

static void logger_rotate(void) {
    LOG_FD_CLOSE(s_logger_cfg.logfd);

    if(s_logger_cfg.maxlogcnt < 2) {
        LOG_FILE_REOPEN(s_logger_cfg.logfd, s_logger_cfg.curlogpath);
        s_logger_cfg.curlogsize = 0;
        return;
    }

    logger_rotate_iter(0);
}

static void do_logger_print(char *pbuf, int32_t len) {
    if(s_logger_cfg.logfd < 0) {
        printf("%s", pbuf);
        return;
    }

    _logger_write(s_logger_cfg.logfd, pbuf, len);
    s_logger_cfg.curlogsize += len;
    if(s_logger_cfg.curlogsize > s_logger_cfg.maxlogsize) {
        logger_rotate();
    }
}
void logger_print(enum e_logger_level_t level, const char *fmt, ...) {
    if(level < s_loglev) { return; }

    struct tm _lg_tm;
    uint32_t cur_s, cur_ms;
    slog_curtime(&cur_s, &cur_ms);
    time_t cur_time = (time_t)cur_s;
    ts2localtime(&cur_time, &_lg_tm);

    bsize = snprintf(buf, sizeof(buf), "%c:%02u-%02u %02u:%02u:%02u.%03u:%d:",
        loglev_text[level],
        _lg_tm.tm_mon + 1, _lg_tm.tm_mday,
        _lg_tm.tm_hour, _lg_tm.tm_min, _lg_tm.tm_sec, cur_ms,
        s_curpid);

    va_list ap;
    va_start(ap, fmt);
    bsize += vsnprintf(buf + bsize, sizeof(buf) - bsize, fmt, ap);
    va_end(ap);

    do_logger_print(buf, bsize);
}

strulog_t strulog_new(enum e_logger_level_t lev, const char *f, int32_t line) {
    if(lev < s_loglev) {
        bsize = 0;
        return s_strulog;
    }

    struct tm _lg_tm;
    uint32_t cur_s, cur_ms;
    slog_curtime(&cur_s, &cur_ms);
    time_t cur_time = (time_t)cur_s;
    ts2localtime(&cur_time, &_lg_tm);

    bsize = snprintf(buf, sizeof(buf), "%c:%02u-%02u %02u:%02u:%02u.%03u:%d:%s:%d",
        loglev_text[lev],
        _lg_tm.tm_mon + 1, _lg_tm.tm_mday,
        _lg_tm.tm_hour, _lg_tm.tm_min, _lg_tm.tm_sec, cur_ms,
        s_curpid, f, line);

    return s_strulog;
}
static strulog_t ptr(char *key, void *val) {
    if(bsize > 0) {
        bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=%p", key, val);
    }
    return s_strulog;
}
static strulog_t string(char *key, char *val) {
    if(bsize > 0) {
        bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=%s", key, val);
    }
    return s_strulog;
}
static strulog_t strsize(char *key, char *val, int32_t vsize) {
    if(bsize > 0) {
        bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=%.*s", key, vsize, val);
    }
    return s_strulog;
}
static strulog_t hex(char *key, void *val, int32_t vsize) {
    if(bsize <= 0) { return s_strulog; }
    if(vsize <= 0) {
        bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=nil", key);
        return s_strulog;
    }
    int32_t i;
    uint8_t *uv = (uint8_t *)val;
    bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=0x%02x", key, uv[0]);
    for(i=1; i < vsize; ++i) {
        bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%02x", uv[i]);
    }
    return s_strulog;
}
static strulog_t int16(char *key, int16_t val) {
    if(bsize > 0) {
	bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=%"PRId16, key, val);
    }
    return s_strulog;
}
static strulog_t int32(char *key, int32_t val) {
    if(bsize > 0) {
        bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=%"PRId32, key, val);
    }
    return s_strulog;
}
static strulog_t int64(char *key, int64_t val) {
    if(bsize > 0) {
        bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=%"PRId64, key, val);
    }
    return s_strulog;
}
static strulog_t uint16(char *key, uint16_t val) {
    if(bsize > 0) bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=%"PRIu16, key, val);
    return s_strulog;
}
static strulog_t uint32(char *key, uint32_t val) {
    if(bsize > 0) {
        bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=%"PRIu32, key, val);
    }
    return s_strulog;
}
static strulog_t uint64(char *key, uint64_t val) {
    if(bsize > 0) {
        bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=%"PRIu64, key, val);
    }
    return s_strulog;
}
//static strulog_t boolean(char *key, bool val) {
//    if(bsize > 0) bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",%s=%d", key, val ? 1 : 0);
//    return s_strulog;
//}
static void post(char *msg) {
    if(bsize > 0) {
        bsize += snprintf(buf + bsize, sizeof(buf) - bsize, ",msg=%s\n", msg ? msg : ".");
        do_logger_print(buf, bsize);
    }
}

