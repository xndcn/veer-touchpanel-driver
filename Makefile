CC = arm-linux-gcc
TARGET = ctp
$(TARGET): ctp.c
	$(CC) -o $(TARGET) ctp.c -lm -static

clean:
	rm $(TARGET)
