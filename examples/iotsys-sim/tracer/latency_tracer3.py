#!/bin/python

import os
import csv
import re
import sqlite3
import numpy as np

from collections import defaultdict

INTERVAL = 10
NR_OF_INTERVALS = 30

http_begin = "06006304 801E0001 \w{4}1F90 \w{8} 00000000 A0023840 \w{8} 020405A0 0402080A 0\w{7} 00000000 01030304"
#http_end = "06006304 001E\w{2}01 1F90\w{4} 0000\w{4} \w{8} 50100030 \w{4}0000"
http_end = "1F90\w{4} 0000\w{4} \w{8} 50100030 \w{4}0000"

coap_begin = "16331633 000E8A\w{2} 41019B\w{2} 9168"
coap_end = "11006304 801E0001 16331633 \w{8} 62459B\w{2} 4\w{3}D102*"

coap_128end = "E0800\w{3} 0D636363 63646464 64656565 65666666 66676767 67686868 68"

def parse(filename, begin_pattern, end_pattern, requester, responders, interval=INTERVAL) :

	print("Now parsing: " +filename)

	startpoints = []
	data = []

	endsfound = 0

	lastend = 0

	with open (filename, 'rb') as f :
		reader = csv.reader (f, delimiter = '\t')
		for row in reader :
			if requester in row[1]:
				if (p_begin.search(row[3])) :
					timestamp = int(row[0])
					# print ("pot_start: " + str(timestamp))

					if startpoints :
						# print("laststart: " + str(startpoints[-1]))
						# print("result: " + str(timestamp - startpoints[-1]))
						# print("guard: "+ str(((interval * 1000) - 3000)))
						if (timestamp - int(startpoints[-1])) > ((interval * 1000) - 3000) :
							if lastend > 0:
								# print ("PINNED end: " + str(lastend))
								value = int(lastend) - startpoints[-1]
								if value <= 0:
									print "negative value not possible!"
									print ("lastend: " + str(lastend) + " - startpoint: " +str(startpoints[-1]))
									print ("current: " +str(timestamp))
									print filename
									exit(1)
	
								data.append(value)
								# add startpoint always (without end), remove again at the end
								startpoints.append(timestamp)
								# print ("PINNED start: " + str(timestamp))

								# break if enough data gathered
								if len(data) == NR_OF_INTERVALS :
									startpoints.pop()
									break;
						#else :
						#	if not data :
						#		print ("PINNED start (overwrite): " + str(timestamp))
						#		startpoints[-1] = timestamp


					else :
						# print ("PINNED start (first): " + str(timestamp))
						startpoints.append(timestamp)

					# laststart = timestamp

			for responder in responders:
				if responder in row[1] and requester in row[2]:
					if (p_end.search(row[3])) :
						endsfound += 1
						timestamp = int(row[0])
						if startpoints :
							lastend = timestamp
							# print("end: " + str(timestamp))

		if not len(data) == NR_OF_INTERVALS :
			# print ("PINNED end: " + str(lastend))
			data.append(int(lastend) - startpoints[-1])

	if len(startpoints) == 0 :
		print("no startpoints found for " + filename + "!")
		return []

	# removing last starpoint, will else cause inconsistencies with calculations (is without end)
	# print ("will REMOVE last found startpoint: " +str(startpoints[-1]))
	# startpoints.pop()

	if len(data) != NR_OF_INTERVALS :
		print("data size off: found "+ str(len(data)) +" intervals for " + filename + "!")
		print filename
		exit(1)

	# print (startpoints[-1] - startpoints[0])
	timespan = (startpoints[-1] - startpoints[0] + 500) / 1000;
	packets_in_timespan = (timespan / interval) + 1

	# print("beginning: "+ str(startpoints[0]) +" - end: "+ str(startpoints[-1]))
	# print("timespan orignal: "+ str(startpoints[-1] - startpoints[0]))
	# print("timespan: " + str(timespan))
	# print("expected packets: " + str(packets_in_timespan))
	mean_int_starts = 0

	if packets_in_timespan != len(data) :
		print(filename +": expected other packet count ("+ str(packets_in_timespan) + " : " + str(len(data)) + ")")
		intervals = []
		for i in range(1, len(startpoints)) :
			intervals.append(startpoints[i] - startpoints[i - 1])
			# print("startpoint " + str(startpoints[i]) + " is " + str(intervals[-1]) + " after " + str(startpoints[i - 1]))

		print("	data for intervals between found beginnings: " )
		print("		mean of intervals: "+ str(np.mean(intervals)))
		mean_int_starts = np.mean(intervals)
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
	print("nr of readings for " + filename + ": " + str(len(data)) + ", mean: " + str(np.mean(data))  + ", std: " + str(np.std(data)))
	# print("----")

	return data, mean_int_starts;

# load data:
conn = sqlite3.connect("coap.db")

c = conn.cursor()

# c.execute('''CREATE TABLE durations3 (binding text, hops int, payload int, requests int, duration int, iteration int, comments text)''')

filenames = []

for file in os.listdir("./"):
    if file.startswith("pdump-") and file.endswith(".txt"):
    	filenames.append(file)

for filename in filenames:

	regex = re.search('pdump-(\w*)-(\d*)b-rdc-hop(\d*)_(\d?)(_?\d*?sec)?.txt', filename)

	binding = regex.group(1)
	payload = regex.group(2)
	hop = regex.group(3)
	iteration = regex.group(4)

	if regex.group(5):
		req_interval = int(re.search('_(\d*)sec', regex.group(5)).group(1))
	else:
		req_interval = 10

	responder = "1"
	requester = "3"
	comment = ""
	requests = "1"

	if ("http" in binding):
		p_begin = re.compile(http_begin)
		p_end = re.compile(http_end)
	else:
		p_begin = re.compile(coap_begin)
		if ("128" in payload) or ("64" in payload):
			p_end = re.compile(coap_128end)
		else:
			p_end = re.compile(coap_end)

	intervals = []

	if req_interval != 10:
		intervals, mean_int_starts = parse(filename, p_begin, p_end, requester, responder, req_interval)
		comment += (", "+ str(req_interval) +"s interval" if comment else ""+ str(req_interval) +"s interval")
	else:
		intervals, mean_int_starts = parse(filename, p_begin, p_end, requester, responder)

	if mean_int_starts != 0:
		comment_to_add = "racing clock (mean time between startpoints=" + str(mean_int_starts) + ")"
		comment += (", "+ comment_to_add if comment else comment_to_add)

	intervals = intervals[-25:-1]

	# let user add comment
	# try:
	# 	user_comment = raw_input("Enter comment and/or press enter: ")
	# 	if user_comment:
	# 		comment += (", " + user_comment)
	# except EOFError:
	# 	pass

	print ("comment: " + comment)

	for duration in intervals:
		c.execute('INSERT INTO durations3 VALUES (?,?,?,?,?,?,?)', (binding, hop, payload, requests, duration, iteration, comment))
		# print (filename + ": " + str(duration) + ", " + str(binding) + ", " + str(hop) + ", " + str(payload) + ", " + str(iteration) + ", " + str(comment))

	print("=== == ===")	

conn.commit()
conn.close()