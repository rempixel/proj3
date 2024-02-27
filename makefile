all: addem life

addem: addem.c
	gcc -o addem addem.c -lpthread

life: life.c
	gcc -o life life.c -lpthread

clean:
	rm -f addem.o addem life.o life