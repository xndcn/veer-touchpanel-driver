CC = arm-linux-gcc
TARGET = ctp
$(TARGET):
	$(CC) -o $(TARGET) ctp.c -lm -static

clean:
	rm $(TARGET)
