all: liballoc.so

liballoc.so: alloc.c
	gcc -shared -g -O0 -Wl,-soname,liballoc.so.1 -ldl -o liballoc.so.1.0 alloc.c -fPIC
	@rm -f liballoc.so.1 liballoc.so
	ln -s liballoc.so.1.0 liballoc.so.1
	ln -s liballoc.so.1 liballoc.so
