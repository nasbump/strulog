## strulog
strulog: structured-logger。一款结构化、链式调用的日志库。

## 初衷
实在厌倦了日志打印时不停的写`%d,%u,%lu,%ld,%llu,%lld,%p`，还要一一对应各字段。
于是参考golang中的zerolog库，开发了这个`strulog`，目的：
- 不需要写`%d`等各种格式占位符
- 不需要花精力考虑字段对齐
- 链式调用，轻松上手

## 用法
```
## 初始化日志库
logger_init(LOGGER_TRACE, 1<<20, 1, "/dev/null");

## 开始打印
slogw.string("str", "abcd").int32("iv32", 3).uint64("uv64", 5252355).post("ok");

## 具体可参考 test_strulog.c
``` 

## 性能对比
性能肯定有降低，但不明显:
```
## 常规打印百万行日志，耗时0.93秒
$ time ./t 1000000

real    0m0.932s
user    0m0.820s
sys     0m0.113s

## 链式打印百万行日志，耗时1.24秒
$ time ./t 1000000 test-by-linked

real    0m1.243s
user    0m1.142s
sys     0m0.101s
```
