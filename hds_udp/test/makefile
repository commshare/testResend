OBJS = $(wildcard ../obj/*.o)
a.out:main.o
	g++ -o a.out -g main.o $(OBJS) -lpthread -luv -L/usr/local/libuv -lrt
main.o:
	g++ -c main.cpp -g -Wall  -I/usr/local/boost ../kernel/ -I/usr/local/libuv/include
clean:
	rm *.o
	rm a.out

