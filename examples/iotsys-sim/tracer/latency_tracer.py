#!/bin/python

import csv
import re
import sqlite3
import numpy as np

from collections import defaultdict

intervals = defaultdict(list)
intervals = defaultdict(list)

INTERVAL = 10
NR_OF_INTERVALS = 55

http_begin = "06006304 801E0001 \w{4,4}1F90 \w{8,8} 00000000 A0023840 \w{8,8} 020405A0 0402080A 0\w{7,7} 00000000 01030304"
http_end = "1F90\w{4,4} 0000\w{4,4} \w{8,8} 50100030 \w{4,4}0000"

coap_begin = "11006304 801E0001 16331633 000E8\w{3,3} 41019\w{3,3} 9168"
coap_end = "16331633 001180\w{2,2} 6245\w{4,4} 4101D102 68"

coap64_begin = "11006304 801E0001 16331633 0018A\w{3,3} 41019\w{3,3} 9B736D61 72742D6D 65746572"
coap64_end = "E07\w{5,5} .*\w{2,2}656566 66666667 67676768 686868"

coap128_end = "E0\w{6,6} .*\w{2,2}656565 65666666 66676767 67686868 68"

def parse(prefix, begin_pattern, end_pattern, requester, responder, interval=INTERVAL) :

	startpoints = []
	data = []

	startsfound = 0
	endsfound = 0

	lastend = 0

	with open ("pdump-"+ prefix +".txt", 'rb') as f :
		reader = csv.reader (f, delimiter = '\t')
		for row in reader :
			if requester in row[2] and responder in row[3]:
				if (p_begin.search(row[4])) :
					startsfound += 1
					timestamp = int(row[1])
					# print ("pot_start: " + str(timestamp))

					if startpoints :
						# print("laststart: " + str(startpoints[-1]))
						# print("result: " + str(timestamp - startpoints[-1]))
						# print("guard: "+ str(((interval * 1000) - 750)))
						if (timestamp - int(startpoints[-1])) > ((interval * 1000) - 750) :
							# print ("PINNED end: " + str(lastend))
							data.append(int(lastend) - startpoints[-1])
							# add startpoint always (without end), remove again at the end
							startpoints.append(timestamp)
							# print ("PINNED start: " + str(timestamp))

							# break if enough data gathered
							if len(data) == NR_OF_INTERVALS :
								break;
						else :
							if not data :
								# print ("PINNED start (overwrite): " + str(timestamp))
								startpoints[-1] = timestamp


					else :
						# print ("PINNED start (first): " + str(timestamp))
						startpoints.append(timestamp)

					# laststart = timestamp

			if responder in row[2] and requester in row[3]:
				if (p_end.search(row[4])) :
					endsfound += 1
					timestamp = int(row[1])
					if startpoints :
						lastend = timestamp
						# print("end: " + str(timestamp))

	# removing last starpoint, will else cause inconsistencies with calculations (is without end)
	# print ("will REMOVE last found startpoint: " +str(startpoints[-1]))
	startpoints.pop()

	if len(startpoints) == 0 :
		print("no startpoints found for " + prefix + "!")
		return []

	if len(data) != NR_OF_INTERVALS :
		print("data size off: found "+ str(len(data)) +" intervals for " + prefix + "!")

	# print (startpoints[-1] - startpoints[0])
	timespan = (startpoints[-1] - startpoints[0] + 500) / 1000;
	packets_in_timespan = (timespan / interval) + 1

	# print("beginning: "+ str(startpoints[0]) +" - end: "+ str(startpoints[-1]))
	# print("timespan orignal: "+ str(startpoints[-1] - startpoints[0]))
	# print("timespan: " + str(timespan))
	# print("expected packets: " + str(packets_in_timespan))

	if packets_in_timespan != len(data) :
		print(prefix +": expected other packet count ("+ str(packets_in_timespan) + " : " + str(len(data)) + ")")
		intervals = []
		for i in range(1, len(startpoints)) :
			intervals.append(startpoints[i] - startpoints[i - 1])
			# print("startpoint " + str(startpoints[i]) + " is " + str(intervals[-1]) + " after " + str(startpoints[i - 1]))

		print("	data for intervals between found beginnings: " )
		print("		mean of intervals: "+ str(np.mean(intervals)))
		print("		std of intervals: "+ str(np.std(intervals)))
		print("		var of intervals: "+ str(np.var(intervals)))
		# print("	showing boxplot...")
		# fig, ax1 = plt.subplots()
		# plt.boxplot(intervals);
		# ax1.grid(axis='y')
		# plt.ylabel('time(ms)')
		# plt.title('Distribution of found requests for ' +prefix+' - please aknowledge')
		# plt.show()
		# print("starts found: " + str(startsfound))
		# print("ends found: " + str(endsfound))
		# print("beginning at: " + str(startpoints[0]))
		# print("ends at: " + str(startpoints[-1]))
		# print("spans: " + str(startpoints[-1] - startpoints[0]))
		# print("number of startpoints: " + str(len(startpoints)))
		# print("number of endpoints: " + str(len(data)))
	print("nr of readings for " + prefix + ": " + str(len(data)) + ", mean: " + str(np.mean(data))  + ", std: " + str(np.std(data)))
	# print("----")

	return data;

# load data:


conn = sqlite3.connect("coap.db")

c = conn.cursor()

c.execute('''CREATE TABLE durations1 (binding text, hops int, RDC int, payload int, duration int, iteration int, comments text)''')

filenames = [
"http", "http-hop4", "http-rdc", "http-rdc-hop4",
"http-64b", "http-64b-hop4", "http-64b-rdc", "http-64b-rdc-hop4",
"http-128b", "http-128b-hop4", "http-128b-rdc", "http-128b-rdc-hop4",

"coap", "coap-hop4", "coap-rdc", "coap-rdc-hop4",
"coap-64b", "coap-64b-hop4", "coap-64b-rdc", "coap-64b-rdc-hop4",
"coap-128b","coap-128b-hop4", "coap-128b-rdc", "coap-128b-rdc-hop4"
]

intervals["http-rdc-hop4"] = 20
intervals["http-64b-hop4"] = 20
intervals["http-64b-rdc-hop4"] = 20
intervals["http-128b-rdc-hop4"] = 20

for filename in filenames:

	if ("hop4" in filename):
		hop = 4
		if ("http" in filename):
			responder = "3"
		else:
			responder = "4"
	else:
		hop = 1
		responder = "2"

	rdc =  (1 if "rdc" in filename else 0)
	payload = (64 if "64b" in filename else 2)
	payload = (128 if "128b" in filename else payload)
	iteration = 1
	comment = ("clock skew?" if "http" in filename else "")

	if ("http" in filename):
		p_begin = re.compile(http_begin)
		p_end = re.compile(http_end)
		binding = "http";
	elif ("coap-128b" in filename):
		p_begin = re.compile(coap64_begin)
		p_end = re.compile(coap128_end)
		binding = "coap";
	elif ("coap-64b" in filename):
		p_begin = re.compile(coap64_begin)
		p_end = re.compile(coap64_end)
		binding = "coap";
	elif ("coap" in filename):
		p_begin = re.compile(coap_begin)
		p_end = re.compile(coap_end)
		binding = "coap";
	else:
		print ("No pattern set for filename. Aborting!")
		exit(0)

	if intervals[filename]:
		intervals[filename] = parse(filename, p_begin, p_end, "1", responder, intervals[filename])
		comment += (", 20s interval" if comment else "20s interval")
	else:
		intervals[filename] = parse(filename, p_begin, p_end, "1", responder)

	# let user add comment
	# try:
	# 	user_comment = raw_input("Enter comment and/or press enter: ")
	# 	if user_comment:
	# 		comment += (", " + user_comment)
	# except EOFError:
	# 	pass

	print ("comment: " + comment)

	for duration in intervals[filename]:
		c.execute('INSERT INTO durations1 VALUES (?,?,?,?,?,?,?)', (binding, hop, rdc, payload, duration, iteration, comment))

	print("=== == ===")	

conn.commit()
conn.close()