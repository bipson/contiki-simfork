#!/bin/python

""" Latency tracer for group communication (blossom type)

This file is currently single purpose to write to a db (making it more generic
would have cost more time than just changing it for other purposes).
If it fails, most certainly the pattern 'coap_begin' is not well suited for your
message, enable the debug output (uncomment print statements in parse()) for
more detailed debugging.
"""

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
	"""Parse the given radio log.
	
	Keyword arguments:
	filename -- the name of the file to parse
	begin_pattern -- the pattern that is used to identify the message (re)
	requester -- the id of the node sending the request
	responders -- ids of the nodes inside the blossom
	other_responders -- ids of the receiving nodes _not_ inside the blossom
	interval -- the interval used (between successive requests)

	Parse for given pattern begin_pattern being sent by the requester to responders,
	return duration between first request and last reception of request, as well as
	missed requests (requests that were not received by all defined responders).
	other_responders defines nodes outside of the blossom that still must be reached
	by the request, else the miss_count will be increased by one (i'm not sure anymore
	now why I added this constraint, most certainly it was used to determine if all
	blossoms are fully working)
	interval of the requests is used to distinguish subsequent requests from each other
	(as they are typically resent multiple times)

	In difference to the other latency tracers, there is no 'response', thus the
	endpoint of a transaction is when the last responder received the message
	"""

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

# search for all pdump-*,txt files in current folder, exclude _bak.txt files
for file in os.listdir("./"):
    if file.startswith("pdump-") and file.endswith(".txt"):
    	if file.endswith("_bak.txt"):
    		continue
    	filenames.append(file)

# parse every file seperately
for filename in filenames:

	# get simulation parameters from filename
	regex = re.search('pdump-(\w*)-(\d*)b-rdc-hop(\d*)-(mcast|ucast)(\d?)_(\d?).txt', filename)

	binding = regex.group(1)
	payload = regex.group(2)
	hop = regex.group(3)
	mcast = regex.group(4)
	topology = regex.group(5)
	iteration = regex.group(6)

	mcast = 1 if "mcast" in mcast else 0

	# define requesters and responders/recipients for parsing, as well as some values valid for all simulations
	responders = [2,3,4,5]
	if topology == "4":
		add_responders = [8,9,10,11,14,15,16,17]
	else:
		add_responders = []
	requester = "6"
	comment = ""
	requests = "1"
	rdc = "1"

	# set the pattern that identifies the request
	p_begin = re.compile(coap_begin)

	intervals = []

	# parse file, save durations and missed requests
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

	# save values to db
	for duration in intervals:
		c.execute('INSERT INTO durations_group VALUES (?,?,?,?,?,?,?,?,?,?)', (binding, hop, payload, requests, rdc, mcast, topology, duration, iteration, comment))
		# 	print (filename + ": " + str(duration) + ", " + str(binding) + ", " + str(hop) + ", " + str(payload) + ", " + str(iteration) + ", " + str(comment), topology)

	print("=== == ===")	

conn.commit()
conn.close()