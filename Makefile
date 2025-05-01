CFLAGS = -o2 -std=gnu99
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
TARGET = t
 
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ 
 
%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@
 
# 清理规则 
clean:
	rm -f $(OBJS) $(TARGET)
