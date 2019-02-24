import subprocess
import time
import sys

if len(sys.argv) != 2:
	print("Usage: ./run_client.py <ip>")
	exit()

for packet_size in [1, 16, 128, 1024]:
	# packets = int(1024 / packet_size) * 10000000
	packets = 10000000
	print("\n============================\nPACKETS: " + str(packets) + "\nPACKET_SIZE: " + str(packet_size) + "\n============================")
	# print("\n==================\nSEND_FILE\n==================")
	# subprocess.call("./benchmark_af_socket_send_file_client.out " + sys.argv[1] + " 8080 " + str(packets) + " " + str(packet_size), shell=True)
	# time.sleep(4)
	print("\n==================\nDEFAULT_SOCKET\n==================")
	subprocess.call("./benchmark_af_socket_client.out " + sys.argv[1] + " 8080 " + str(packets) + " " + str(packet_size), shell=True)
	time.sleep(4)
	print("\n==================\nBOOST_SOCKET\n==================")
	subprocess.call("./benchmark_boost_client.out " + sys.argv[1] + " 8080 " + str(packets) + " " + str(packet_size), shell=True)
	time.sleep(4)
