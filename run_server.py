import subprocess
import time
import sys

if len(sys.argv) != 1:
	print("Usage: ./run_server.py")
	exit()

subprocess.call("./benchmark_af_socket_send_file_server.out 8080", shell=True)
subprocess.call("./benchmark_af_socket_server.out 8080", shell=True)
subprocess.call("./benchmark_boost_server.out 8080", shell=True)
