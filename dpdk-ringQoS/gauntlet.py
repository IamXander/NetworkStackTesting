import subprocess
import sys
import json

def flattenjson( b, delim ):
	val = {}
	for i in b.keys():
		if isinstance( b[i], dict ):
			get = flattenjson( b[i], delim )
			for j in get.keys():
				val[ i + delim + j ] = get[j]
		elif isinstance( b[i], list ):
			get = flattenjson( {str(v): k for v, k in enumerate(b[i])}, delim )
			for j in get.keys():
				val[ i + delim + j ] = get[j]
		else:
			val[i] = b[i]

	return val

generalParams = {
	"rate": str(1000 * 1000 * 1000 * 4), # 4GBps
	"mtu": str(4096),
	"n_pipes_per_subport": str(4096),
	"qsize": ["128", "128", "128", "128"],
	"dequeueSize": str(128*2),
	"sleepTime": "1"
}

subportParams = [
	{
		"tb_rate": generalParams["rate"], #1250000000
		"tb_size": "1000000",
		"tc_rate": [generalParams["rate"], generalParams["rate"], generalParams["rate"], generalParams["rate"]], #{1250000000, 1250000000, 1250000000, 1250000000},
		"tc_period": "10",
	}
]


generalParams['n_subports_per_port'] = str(len(subportParams))

pipeProfiles = [
	{
		"tb_rate": generalParams["rate"], #305175
		"tb_size": "1000000",
		"tc_rate": [generalParams["rate"], generalParams["rate"], generalParams["rate"], generalParams["rate"]], #305175
		"tc_period": "40",
		"wrr_weights": ["1", "1", "1", "1",  "1", "1", "1", "1",  "1", "1", "1", "1",  "1", "1", "1", "1"],
	}
]

generatorParams = [
	{
		"packetsToGenerate": "524288",
		"packetSize": "16",
		"queueSize": "128",
		"burstSize": "128",
		"packetsToDequeue": "128",

		"subport": "0",
		"pipe": "0",
		"traffic_class": "0",
		"queue": "0"
	},
	{
		"packetsToGenerate": "524288",
		"packetSize": "16",
		"queueSize": "128",
		"burstSize": "128",
		"packetsToDequeue": "128",

		"subport": "0",
		"pipe": "0",
		"traffic_class": "0",
		"queue": "0"
	}
]

def setRate(rate):
	generalParams['rate'] = rate
	subportParams[0]['tb_rate'] = rate
	subportParams[0]['tc_rate'] = [rate, rate, rate, rate]
	pipeProfiles[0]['tb_rate'] = rate
	pipeProfiles[0]['tc_rate'] = [rate, rate, rate, rate]


def run():
	subportParamsList = []
	pipeProfilesList = []
	generatorPramsList = []

	for param in subportParams:
		subportParamsList.append("-s")
		subportParamsList.append(param["tb_rate"])
		subportParamsList.append(param["tb_size"])
		subportParamsList.extend(param["tc_rate"])
		subportParamsList.append(param["tc_period"])

	for prof in pipeProfiles:
		pipeProfilesList.append("-p")
		pipeProfilesList.append(prof["tb_rate"])
		pipeProfilesList.append(prof["tb_size"])
		pipeProfilesList.extend(prof["tc_rate"])
		pipeProfilesList.append(prof["tc_period"])
		pipeProfilesList.extend(prof["wrr_weights"])

	for param in generatorParams:
		generatorPramsList.append("-g")
		generatorPramsList.append(param["packetsToGenerate"])
		generatorPramsList.append(param["packetSize"])
		generatorPramsList.append(param["queueSize"])
		generatorPramsList.append(param["burstSize"])
		generatorPramsList.append(param["packetsToDequeue"])
		generatorPramsList.append(param["subport"])
		generatorPramsList.append(param["pipe"])
		generatorPramsList.append(param["traffic_class"])
		generatorPramsList.append(param["queue"])

	coresString = ""
	for i in range(len(generatorParams)+1):
		coresString += str(i) + ','
	coresString = coresString[0:-1]

	args = ["sudo", "./build/NetstackTesting",
		"-l", coresString, "--",
		"-d", generalParams["dequeueSize"],
		"-st", generalParams["sleepTime"],
		"-c", generalParams["rate"], generalParams["mtu"], generalParams["n_subports_per_port"], generalParams["n_pipes_per_subport"]]

	args.extend(generalParams["qsize"])
	args.extend(generatorPramsList)
	args.extend(subportParamsList)
	args.extend(pipeProfilesList)

	generalParams['subportParams'] = subportParams
	generalParams['generatorParams'] = generatorParams
	generalParams['pipeProfiles'] = pipeProfiles

	# print("Running with command\n" + ' '.join(args))

	output = subprocess.check_output(args, stderr=subprocess.STDOUT).decode("utf-8")

	data = output.split("@@@DATA@@@\n")
	if (len(data) == 1):
		print("Could not split")
	
	generalParams['results'] = json.loads(data[1])

	# resultsAll = data[1].split('\n')

	# print(output)
	# print(generalParams)
	csv = flattenjson(generalParams, '__')
	# print(flattenjson(generalParams, '__'))

	header = []
	body = []
	for key in sorted(csv):
		header.append(key)
		body.append(csv[key])
	return header, [str(b) for b in body];

if len(sys.argv) <= 1:
	print("specify a test")
	exit(0)

# vary packetSize
# limit dequeueSize
if sys.argv[1] == 'ds':
	generatorParams[1]['queue'] = str(1)
	generalParams['dequeueSize'] = str(64)
	results = []
	for i in [16, 32, 64, 128, 256, 512, 1024, 2048, 4096]:
		generatorParams[0]['packetSize'] = str(i)
		for j in [16, 32, 64, 128, 256, 512, 1024, 2048, 4096]:
			generatorParams[1]['packetSize'] = str(j)
			header, runResults = run()
			results.append(runResults)
			print(i, j)
	print(','.join(header))
	print(','.join(results[0]))

	meaningfulValues = ['generatorParams__0__packetSize', 'results__Core0__dropped', 'results__Core0__survived', 'results__Core0__total',
		'generatorParams__1__packetSize', 'results__Core1__dropped', 'results__Core1__survived', 'results__Core1__total', 'results__duration']
	meaningfulLocations = [header.index(i) for i in meaningfulValues]

	printableResults = []

	for result in results:
		printableResults.append([result[location] for location in meaningfulLocations])
	
	print(','.join(meaningfulValues))
	for result in printableResults:
		print(','.join(result))
elif sys.argv[1] == 'r':
	# vary packetSize
	# limit rate
	generatorParams[1]['queue'] = str(1)
	generalParams['dequeueSize'] = str(256)
	setRate(str(10000000))
	results = []
	for i in [16, 32, 64, 128, 256, 512, 1024, 2048, 4096]:
		generatorParams[0]['packetSize'] = str(i)
		for j in [16, 32, 64, 128, 256, 512, 1024, 2048, 4096]:
			generatorParams[1]['packetSize'] = str(j)
			header, runResults = run()
			results.append(runResults)
			print(i, j)
	print(','.join(header))
	print(','.join(results[0]))

	meaningfulValues = ['generatorParams__0__packetSize', 'results__Core0__dropped', 'results__Core0__survived', 'results__Core0__total',
		'generatorParams__1__packetSize', 'results__Core1__dropped', 'results__Core1__survived', 'results__Core1__total', 'results__duration']
	meaningfulLocations = [header.index(i) for i in meaningfulValues]

	printableResults = []

	for result in results:
		printableResults.append([result[location] for location in meaningfulLocations])
	
	print(','.join(meaningfulValues))
	for result in printableResults:
		print(','.join(result))