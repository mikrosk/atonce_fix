TARGET = calc_chs

default: $(TARGET)

$(TARGET): calc_chs.c
	$(CC) -O2 -Wall -o $@ $^

.PHONY: clean
clean:
	rm -f $(TARGET) *~
