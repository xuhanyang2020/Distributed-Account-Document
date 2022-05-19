from distutils.command.config import LANG_EXT
from cv2 import sort
import matplotlib.pyplot as plt
import sys

node_num = int(sys.argv[1])
sequence = [i for i in range(1, node_num + 1)]

# dictionary to store deliver time for each message
# key : ID of each message (ServerID + message generated time)
# value: list, with deliver times at different servers
trans_stat = {}

for i in sequence:
    file = "node{}.txt".format(i)
    f = open(file)
    for line in f:
        l = line.split()
        pk = l[0] + l[1]
        if pk not in trans_stat:
            trans_stat[pk] = []
        # if (int(l[2]) - int(l[1])) < 1000000:
        trans_stat[pk].append((int(l[2]) - int(l[1])));
            # print((int(l[2]) - int(l[1])))

tmp = []
for key in trans_stat:
    if len(trans_stat[key]) > 0:
        tmp.append(max(trans_stat[key]))

tmp.sort()

# set timeout as 2 times mid process time
mid = int(len(tmp) / 2)
time_out = tmp[mid] * 2
latency = [t / 1000000 for t in tmp if t <= time_out]

# convert the format to CDF
length = len(latency)
ratio = []
for i in range(length):
    ratio.append( i / length )

# plot graph
plt.plot(latency, ratio)
plt.xlabel('latency (ms)')
plt.ylabel('ratio of CDF')
plt.title("3 node 0.5 HZ without failure")
plt.show()