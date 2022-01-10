CC = cc

CFLAGS = -Wall -std=c99 -fcommon

SRCS = main.c editor.c
MAIN = mx


.PHONY: all
.DEFAULT: all
all: $(MAIN)


$(MAIN): 
	$(CC) $(CFLAGS) $(SRCS) -o $(MAIN) 

