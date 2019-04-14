import subprocess
import sys

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

# stderr=subprocess.STDOUT - if you want to get ride of the EAL stuff
# sudo ./build/NetstackTesting -l 0,1 -- -g 100 100 128 2 1 0 0 0 0 -d 5

generalParams = {
	"rate": str(1000 * 1000 * 10), # 4GBps
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
	"tc_rate": [generalParams["rate"], "30517", "30517", "30517"], #{1250000000, 1250000000, 1250000000, 1250000000},
	"tc_period": "10",
}]

generalParams['n_subports_per_port'] = str(len(subportParams))

pipeProfiles = [{
	"tb_rate": generalParams["rate"], #305175
	"tb_size": "1000000",
	"tc_rate": [generalParams["rate"], "30517", "30517", "30517"], #305175
	"tc_period": "40",
	"wrr_weights": ["1", "1", "1", "1",  "1", "1", "1", "1",  "1", "1", "1", "1",  "1", "1", "1", "1"],
}]

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
		"packetSize": "4096",
		"queueSize": "128",
		"burstSize": "128",
		"packetsToDequeue": "128",

		"subport": "0",
		"pipe": "0",
		"traffic_class": "0",
		"queue": "1"
	}
]

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

print("Running with command\n" + ' '.join(args))

output = subprocess.check_output(args).decode("utf-8")

data = output.split("@@@DATA@@@\n")
if (len(data) == 1):
	print("Could not split")

resultsAll = data[1].split('\n')

print(output)
# print(generalParams)
csv = flattenjson(generalParams, '__')
# print(flattenjson(generalParams, '__'))

header = []
body = []
for key in sorted(csv):
	header.append(key)
	body.append(csv[key])
# print(','.join(header))
# print(','.join(body))
# i = 0
# for param in sorted(subportParams):
# 	for key in sorted(param):
# 		header.append(key + "__" + str(i))
# 		body.append(param[key])
# 	i += 1
# i = 0
# for param in sorted(pipeProfiles):
# 	for key in sorted(param):
# 		header.append(key + "__" + str(i))
# 		body.append(param[key])
# 	i += 1
# i = 0
# for param in sorted(generatorParams):
# 	for key in sorted(param):
# 		header.append(key + "__" + str(i))
# 		body.append(param[key])
# 	i += 1
# print(header)
# print(body)