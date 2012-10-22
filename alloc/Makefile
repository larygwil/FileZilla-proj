all: liballoc.so

clean:
	@rm -f liballoc.so
	@rm -f liballoc.so.1
	@rm -f liballoc.so.1.0
	@rm -f pagesize.h
	@rm -f pagesize

pagesize: pagesize.c Makefile
	gcc -o pagesize pagesize.c

pagesize.h: pagesize
	./pagesize > pagesize.h

liballoc.so: alloc.c Makefile pagesize.h
	gcc -shared -g -O2 -Wl,-soname,liballoc.so.1 -ldl -o liballoc.so.1.0 alloc.c -fPIC
	@rm -f liballoc.so.1 liballoc.so
	ln -s liballoc.so.1.0 liballoc.so.1
	ln -s liballoc.so.1 liballoc.so
