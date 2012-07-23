#Simple Makefile

VPATH 		+= src

vpath 		%.h 	src/ui
vpath 		%.c 	src/ui

CC 		:= gcc
CFLAGS 		:= -Wall -Werror -std=gnu99 -O0 -Isrc
CFLAGS 		+= -I /usr/include/libxml2/
CFLAGS 		+= $(shell pkg-config --cflags --libs gtk+-2.0)

LDLIBS 		+= -L /usr/lib -lxml2

#MAIN 		:= ./src/main.c
BINNAME 	:= myweather

SOURCE 		:= $(shell find ./src -type f -name "*.c")
OBJECT 		:= $(patsubst %.c, %.o, $(SOURCE))
#OBJECT 		:= $(subst %.c,%o, $(SOURCE))
#OBJECT 		:= $(foreach n, $(SOURCE), $(basename $(n)).o)


.PHONY: all clean cscope

all: $(OBJECT) main

main: $(OBJECT)
	$(CC) $(CFLAGS) $(LDLIBS) $(HEAD) $(OBJECT) -o $(BINNAME)

#Generator all obj file
$(STATIC): $(OBJECT)
#$(OBJECT): %.o: %.c

cscope:
	find ./src -name "*.c" -o -name "*.h" > cscope.files
	cscope -Rbq

clean:
	find -name "*.o" -exec rm {} \;
	-rm $(BINNAME)
