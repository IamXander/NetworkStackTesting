CXXFALGS = g++ -O2 -g -pthread -std=c++17 -D CORE_COUNT=8 -I include -I boost

default: benchmark_af_socket_server.out benchmark_af_socket_client.out benchmark_boost_server.out benchmark_boost_client.out

clean:
	rm -f *.o
	rm -f *.out

af_socket_driver.o: include/driver.h src/af_socket/driver.cpp
	$(CXXFALGS) src/af_socket/driver.cpp -c -o af_socket_driver.o

boost_driver.o: include/driver.h src/boost/driver.cpp
	$(CXXFALGS) src/boost/driver.cpp -c -o boost_driver.o

cpu.o: include/cpu/cpu.h src/cpu/cpu.cpp
	$(CXXFALGS) src/cpu/cpu.cpp -c -o cpu.o

benchmark_server.o: src/benchmark_server.cpp
	$(CXXFALGS) src/benchmark_server.cpp -c -o benchmark_server.o

benchmark_client.o: src/benchmark_client.cpp
	$(CXXFALGS) src/benchmark_client.cpp -c -o benchmark_client.o

benchmark_af_socket_server.out: cpu.o benchmark_server.o af_socket_driver.o
	$(CXXFALGS) -o benchmark_af_socket_server.out benchmark_server.o cpu.o af_socket_driver.o

benchmark_af_socket_client.out: cpu.o benchmark_client.o af_socket_driver.o
	$(CXXFALGS) -o benchmark_af_socket_client.out benchmark_client.o cpu.o af_socket_driver.o

benchmark_boost_server.out: cpu.o benchmark_server.o boost_driver.o
	$(CXXFALGS) -o benchmark_boost_server.out benchmark_server.o cpu.o boost_driver.o

benchmark_boost_client.out: cpu.o benchmark_client.o boost_driver.o
	$(CXXFALGS) -o benchmark_boost_client.out benchmark_client.o cpu.o boost_driver.o