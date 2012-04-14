CC = arm-linux-gcc
TARGET = ctp
$(TARGET): $(TARGET).c
	$(CC) -o $(TARGET) ctp.c -lm -static

clean:
	rm $(TARGET)
