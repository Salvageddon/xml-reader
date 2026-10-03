srctarget = src/source/xmlReader.c
dlltarget = ./xmlReader.dll
maintarget = src/source/main.c
libtarget = -L.

all: build run

build:
	gcc -shared ${srctarget} -o ${dlltarget} ${libtarget} -llist

run:
	gcc ${maintarget} -o ./test ${libtarget} -llist -lxmlReader
	./test