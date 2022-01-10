TARGET = analyse_mbr

default: $(TARGET)

$(TARGET): analyse_mbr.c
	$(CC) -o $@ $^

.PHONY: clean
clean:
	rm -f $(TARGET) *~
