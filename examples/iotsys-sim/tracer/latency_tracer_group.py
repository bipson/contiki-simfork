#!/bin/python

import os
import csv
import re
import sqlite3
import numpy as np

from collections import defaultdict

INTERVAL = 10
NR_OF_INTERVALS = 32

coap_begin = "FF61\s?6161\s?61"

def parse(filename, begin_pattern, requester, responders, other_responders, interval=INTERVAL) :

	print("Now parsing: " +filename)

	startpoints = []
	data = []
	all_responders_left = responders + other_responders;
	responders_left = responders

	lastend = 0
	miss_count = 0

	with open (filename, 'rb') as f :
		reader = csv.reader (f, delimiter = '\t')
		for row in reader :
			# startpoints are: sent by requester and have begin_pattern
			if requester in row[1] and (begin_pattern.search(row[3])) :
				timestamp = int(row[0])
				# print ("pot_start: " + str(timestamp))

				if startpoints :
					# print("laststart: " + str(startpoints[-1]))
					# print("result: " + str(timestamp - startpoints[-1]))
					# print("guard: "+ str(((interval * 1000) - 3000)))
					if (timestamp - int(startpoints[-1])) > ((interval * 1000) - 3000) :
						if lastend > 0:
							if len(all_responders_left) == 0:
								# print ("PINNED end: " + str(lastend))
								value = int(lastend) - startpoints[-1]
								if value <= 0:
									print "negative value not possible!"
									print ("lastend: " + str(lastend) + " - startpoint: " +str(startpoints[-1]))
									print ("current: " +str(timestamp))
									print filename
									exit(1)
	
								data.append(value)
								# reset responders_left list
								all_responders_left = responders + other_responders
								responders_left = responders
								# add startpoint always (without end), remove again at the end
								startpoints.append(timestamp)
								# print ("PINNED start: " + str(timestamp))

								# break if enough data gathered
								if len(data) == NR_OF_INTERVALS :
									startpoints.pop()
									break;

							# we got a new startpoint, but have not yet reached all responders
							else:
								miss_count += 1
								startpoints[-1] = timestamp
								print "miss: resetting startpoint to", timestamp
								print "resetting responders_left and all_responders_left arrays"
								all_responders_left = responders + other_responders
								responders_left = responders

					#else :
					#	if not data :
					#		print ("PINNED start (overwrite): " + str(timestamp))
					#		startpoints[-1] = timestamp


				else :
					# print ("PINNED start (first): " + str(timestamp))
					startpoints.append(timestamp)

				# laststart = timestamp
			
			# endpoints can happen when:
			# - startpoint exists
			# - begin_pattern occurs
			# - recipient is one of the responders
			if startpoints and not "-" in row[2] :
				recipients = [int(x) for x in row[2].split(',')]
				# print "recipients", recipients
				if any(responder in recipients for responder in all_responders_left) and (begin_pattern.search(row[3])) :
					# print "looking at", recipients
					# only update endtime if one of the observed blossom
					if any(responder in recipients for responder in responders_left):
						# print("end: " + str(timestamp))
						lastend = timestamp
						responders_left = [v for v in responders_left if not v in recipients ]
						# print "blossom responders left:", responders_left

					# remove entry from responders_left
					all_responders_left = [v for v in all_responders_left if not v in recipients ]
					# print "reponders left:", all_responders_left

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

	return data, miss_count

# add db-connection and create table
conn = sqlite3.connect("coap.db")
c = conn.cursor()
# c.execute('''CREATE TABLE durations_group (binding text, hops int, payload int, requests int, rdc int, mcast int, topology int, duration int, iteration int, comments text)''')

filenames = []

for file in os.listdir("./"):
    if file.startswith("pdump-") and file.endswith(".txt"):
    	if file.endswith("_bak.txt"):
    		continue
    	filenames.append(file)

for filename in filenames:

	regex = re.search('pdump-(\w*)-(\d*)b-rdc-hop(\d*)-(mcast|ucast)(\d?)_(\d?).txt', filename)

	binding = regex.group(1)
	payload = regex.group(2)
	hop = regex.group(3)
	mcast = regex.group(4)
	topology = regex.group(5)
	iteration = regex.group(6)

	mcast = 1 if "mcast" in mcast else 0

	responders = [2,3,4,5]
	if topology == "4":
		add_responders = [8,9,10,11,14,15,16,17]
	else:
		add_responders = []
	requester = "6"
	comment = ""
	requests = "1"
	rdc = "1"

	p_begin = re.compile(coap_begin)

	intervals = []

	intervals, missed = parse(filename, p_begin, requester, responders, add_responders)

	if (missed > 0):
		print("missed intervals: ", missed)
		comment += str(missed) + " missed intervals"

	# only take last 30 intervals
	intervals = intervals[-31:-1]

	# let user add comment
	# try:
	# 	user_comment = raw_input("Enter comment and/or press enter: ")
	# 	if user_comment:
	# 		comment += (", " + user_comment)
	# except EOFError:
	# 	pass

	print (filename + ":", len(intervals), np.mean(intervals))

	for duration in intervals:
		c.execute('INSERT INTO durations_group VALUES (?,?,?,?,?,?,?,?,?,?)', (binding, hop, payload, requests, rdc, mcast, topology, duration, iteration, comment))
		# 	print (filename + ": " + str(duration) + ", " + str(binding) + ", " + str(hop) + ", " + str(payload) + ", " + str(iteration) + ", " + str(comment), topology)

	print("=== == ===")	

conn.commit()
conn.close()