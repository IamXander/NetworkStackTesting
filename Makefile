CXXFALGS = g++ -g -pthread -std=c++17 -D CORE_COUNT=8 -I include

default: benchmark
clean:
	rm -f *.o
	rm -f benchmark

cpu.o: include/cpu/cpu.h src/cpu/cpu.cpp
	$(CXXFALGS) src/cpu/cpu.cpp -c -o cpu.o

benchmark.o: src/benchmark.cpp
	$(CXXFALGS) src/benchmark.cpp -c -o benchmark.o

benchmark.out: cpu.o benchmark.o
	$(CXXFALGS) -o benchmark.out benchmark.o cpu.o
