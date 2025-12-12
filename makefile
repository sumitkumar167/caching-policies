CC = g++
CFLAGS = -std=c++11
TARGET = cache 
#CPP_FILES = $(shell ls *.cpp)
#BASE = $(basename$(CPP_FILES))
#OBJS = $(addsuffix .o, $(BASE))
OBJS = main.o lru.o lfu.o cacheus.o #mru.o lfu.o arc.o mq.o lecar.o harc.o exp.o

$(TARGET):$(OBJS)
	rm -f $@
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o:%.cpp
	$(CC) -c -o $@ $< $(CFLAGS)	

clean:
	rm -f $(OBJS) $(TARGET)
