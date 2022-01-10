TARGET = analyse

default: $(TARGET)

$(TARGET): analyse.c
	$(CC) -o $@ $^

.PHONY: clean
clean:
	rm -f $(TARGET) *~
