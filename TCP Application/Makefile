all: cproxy sproxy
cproxy : cproxy.c
	gcc -Wall cproxy.c -o cproxy
sproxy : sproxy.c  
	gcc -Wall sproxy.c -o sproxy
.PHONY : clean 
clean : 
	rm -f *.o
	rm -f sproxy
	rm -f cproxy