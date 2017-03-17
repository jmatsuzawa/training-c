EXEC = line_trace
OBJS = line_trace.o

CROSS_COMPILE ?= arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)g++
CXXFLAGS = -Wall -Wextra -O3

LDLIBS := -lopencv_core -lopencv_highgui -lopencv_imgproc -lm -lpthread

.PHONY: all romfs clean

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

romfs:
	$(ROMFSINST) /usr/bin/$(EXEC)

clean:
	rm -f $(EXEC) $(OBJS)

.SUFFIXES: .cpp .o
.cpp.o:
	$(CC) $(CXXFLAGS) -c $<
