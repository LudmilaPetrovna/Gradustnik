CC:=gcc
CFLAGS=-ggdb3
TARGET:=tftp-server
OBJECTS:=fancylog2.o tftp-server.o


$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $^


clean:
	rm -vf $(OBJECTS) $(TARGET)
