include .env
export $(sed 's/=.*//' .env)

#colors
ccred=\033[0;31m
ccgreen=\033[0;32m
ccyellow=\033[0;33m
ccend=\033[0m

all: clean build run

clean:
	@echo "${ccgreen}Cleaning tests${ccend}"	
	rm -f test.o
	rm -f testapp.o

build:
	#build testset
	${CC} -Wno-format -c test.c -o test.o
	#build testapp
	${CC} test.o ../libBlockAllocator/bin/allocator.a -o testapp.o

run:
	@echo "${ccyellow}Running tests${ccend}"
	./testapp.o
	@echo "${ccgreen}Tests passed${ccend}"