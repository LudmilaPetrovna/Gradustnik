CC=gcc
CFLAGS=-Wall -O0 -ggdb3 -D_DEBUG -pthread
PROTOC:=protoc-c
LIBS:=-lmbedtls -lmbedx509 -lmbedcrypto -lprotobuf-c
TARGET:=pc_version
OBJECTS:=DHT.o ds18b20.o fancylog2.o mmmu.o mumble_app.o Mumble.pb-c.o velo_counter.o

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(^) $(LIBS)

#Mumble.pb-c.c: Mumble.proto
#	$(PROTOC) --c_out=. $^

clean:
	rm -vf $(TARGET) $(OBJECTS)
