all: build tests

tests:
	cd ./libBlockAllocatorTests && make all

build:
	cd ./libBlockAllocator && make all
