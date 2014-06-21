#!/bin/python

import os
import csv
import re
import sqlite3
import numpy as np

from collections import defaultdict

intervals = defaultdict(list)
intervals = defaultdict(list)

INTERVAL = 10
NR_OF_INTERVALS = 10

http_begin = "06006304 801E0001 \w{4}1F90 \w{8} 00000000 A0023840 \w{8} 020405A0 0402080A 0\w{7} 00000000 01030304"
#http_end = "06006304 001E\w{2}01 1F90\w{4} 0000\w{4} \w{8} 50100030 \w{4}0000"
http_end = "1F90\w{4} 0000\w{4} \w{8} 50100030 \w{4}0000"

coap_begin = "11006304 801E0001 16331633 000E8\w{3,3} 41019\w{3,3} 9168"
coap_end = "16331633 001180\w{2,2} 6245\w{4,4} 4101D102 68"

#coap64_begin = "11006304 801E0001 \w{4,4}1633 0027\w{4,4} 4401\w{4,4} 5F035B61 6161613A 3A633330 633A303A 303A\w{4,4} 22163321 6821\w{2,2}"
coap64_begin = "11006304 801E0001 \w{4}1633 002\w{5} 4401\w{4} 5F035B61 6161613A 3A6333\w{2} 633A303A 303A\w{4} 22163321 682\w{3}"
#coap64_end = "E0\w{6,6} .*\w{2,2}656566 66666667 67676768 686868"
#coap64_end = "E08\w{5} 0C\w{6}\w* .*(62\s?){4}(63\s?){4}(64\s?){4}(65\s?){4}(66\s?){4}(67\s?){4}(68\s?){4}"
coap64_end = "E0\w{6} 0.*(64\s?){2}(65\s?){4}(66\s?){4}(67\s?){4}(68\s?){4}"

#coap128_end = "E0\w{6,6} .*\w{2,2}656565 65666666 66676767 67686868 68"
coap128_end = "E0\w{6} 0.(64\s?){2}(65\s?){4}(66\s?){4}(67\s?){4}(68\s?){4}"

def parse(filename, begin_pattern, end_pattern, requester, responders, interval=INTERVAL) :

	print("Now parsing: " +filename)

	startpoints = []
	data = []
	responders_left = list(responders)

	endsfound = 0

	lastend = 0
	miss_count = 0

	with open (filename, 'rb') as f :
		reader = csv.reader (f, delimiter = '\t')
		for row in reader :
			if requester == int(row[2]) and (begin_pattern.search(row[4])) :
					timestamp = int(row[1])
					# print ("pot_start: " + str(timestamp))

					if startpoints :
						# print("laststart: " + str(startpoints[-1]))
						# print("result: " + str(timestamp - startpoints[-1]))
						# print("guard: "+ str(((interval * 1000) - 3000)))
						if (timestamp - int(startpoints[-1])) > ((interval * 1000) - 3000) :
							if lastend > 0:
								if len(responders_left) == 0:
									# print ("PINNED end: " + str(lastend))
									value = int(lastend) - startpoints[-1]
									if value <= 0:
										print "negative value not possible!"
										print ("lastend: " + str(lastend) + " - startpoint: " +str(startpoints[-1]))
										print ("current: " +str(timestamp))
										print filename
										exit(1)

									# print "appending", value
									data.append(value)
									# reset responders_left
									responders_left = list(responders)
									# add startpoint always (without end), remove again at the end
									startpoints.append(timestamp)
									# print ("PINNED start: " + str(timestamp))

									# break if enough data gathered
									if len(data) == NR_OF_INTERVALS :
										startpoints.pop()
										break

								# we got a new startpoint, but have not yet reached all responders
								else:
									miss_count += 1
									startpoints[-1] = timestamp
									print "miss: resetting startpoint to", timestamp
									print "resetting responders_left list"
									responders_left = list(responders)

							# else :
							# 	print ("PINNED start (overwrite): " + str(timestamp))
							# 	startpoints[-1] = timestamp


					else :
						# print ("PINNED start (first): " + str(timestamp))
						startpoints.append(timestamp)

					# laststart = timestamp

			# endpoints can happen when:
			# - startpoint exists exists
			# - begin_pattern occurs
			# - recipient is one of the responders
			if startpoints and not "-" in row[3] :
				recipients = [int(x) for x in row[3].split(',')]
				#print "recipients", recipients
				if any(responder == int(row[2]) for responder in responders) and (requester in recipients) :
					if (end_pattern.search(row[4])) :
						# print ("found match")
						current_responder = int(row[2])
						endsfound += 1
						# this pattern might occur multiple times per request/response, thus we always overwrite the last one
						lastend = int(row[1])
						# print("end: " + str(timestamp))
						# keep track of all responders, strike them out
						if (current_responder in responders_left) :
							# print "removing", current_responder
							responders_left.remove(current_responder)
						# else :
							# print current_responder, "not in", responders_left

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
		exit(0)

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

c.execute('''CREATE TABLE durations2 (binding text, hops int, payload int, requests int, duration int, iteration int, comments text)''')

filenames = []

for file in os.listdir("./"):
    if file.startswith("pdump-") and file.endswith(".txt"):
    	if file.endswith("_bak.txt"):
    		continue
    	filenames.append(file)

for filename in filenames:

	regex = re.search('pdump-(\w*)-(\d*)b-rdc-hop(\d*)-(\d*)req_(\d?)(_?\d*?sec)?.txt', filename)

	binding = regex.group(1)
	payload = regex.group(2)
	hop = regex.group(3)
	requests = regex.group(4)
	iteration = regex.group(5)

	# binding = re.search('pdump-(\w*)-', filename).group(1)
	# payload = re.search('pdump-\w*-(\d*)b', filename).group(1)
	# hop = re.search('-hop(\d*)-', filename).group(1)
	# requests = re.search('-(\d*)req_', filename).group(1)
	# iteration = re.search('_(\d?)(_?\d*?sec)?.txt', filename).group(1)

	if regex.group(6):
		req_interval = int(re.search('_(\d*)sec', regex.group(6)).group(1))
	else:
		req_interval = 10

	if int(hop) == 1:
		if int(requests) == 1:
			responder = [2]
		elif int(requests) == 2:
			responder = [2,3]
		elif int(requests) == 4:
			responder = [2,3,4,5]
	elif int(hop) == 2:
		if int(requests) == 1:
			responder = [7]
		elif int(requests) == 2:
			responder = [7,9]
		elif int(requests) == 4:
			responder = [6,7,8,9]

	# override if http
	if "http" in binding:
		if int(hop) == 2:
			if int(requests) == 1:
				responder = [3]
			elif int(requests) == 2:
				responder = [4,5]

	comment = ""

	if ("http" in binding):
		p_begin = re.compile(http_begin)
		p_end = re.compile(http_end)
	else:
		p_begin = re.compile(coap64_begin)
		p_end = re.compile(coap64_end)

	if req_interval != 10:
		intervals[filename], mean_int_starts = parse(filename, p_begin, p_end, 1, responder, req_interval)
		comment += (", "+ str(req_interval) +"s interval" if comment else ""+ str(req_interval) +"s interval")
	else:
		intervals[filename], mean_int_starts = parse(filename, p_begin, p_end, 1, responder)

	if mean_int_starts != 0:
		comment_to_add = "racing clock (mean time between startpoints=" + str(mean_int_starts) + ")"
		comment += (", "+ comment_to_add if comment else comment_to_add)

	# let user add comment
	# try:
	# 	user_comment = raw_input("Enter comment and/or press enter: ")
	# 	if user_comment:
	# 		comment += (", " + user_comment)
	# except EOFError:
	# 	pass

	print ("comment: " + comment)

	for duration in intervals[filename]:
		c.execute('INSERT INTO durations2 VALUES (?,?,?,?,?,?,?)', (binding, hop, payload, requests, duration, iteration, comment))
		# print (filename + ": " + str(duration) + ", " + str(binding) + ", " + str(hop) + ", " + str(payload) + ", " + str(iteration) + ", " + str(comment))

	print("=== == ===")	

conn.commit()
conn.close()