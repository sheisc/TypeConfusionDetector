CXX=wllvm++ -g -O0 

all:
	${CXX} main.cpp -o main
	extract-bc main -o main.bc 
	opt -mem2reg main.bc -o main.mem2reg.bc
	llvm-dis main.mem2reg.bc -o main.mem2reg.ll


clean:
	rm -rf main *.o *.bc *.s *.so *.ll
