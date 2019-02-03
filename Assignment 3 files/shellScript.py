import matplotlib.pyplot as plt
import os
import numpy as np

#run for 10 times
times = 10

#values of N
NValues = [10, 50, 100]

#input filename
inputFile = "buffer.txt"

#output filename
outputFile = "output.txt"

#compile the file at the beginning
a = os.system("g++ a3_6.cpp")

Vals = []

for q in NValues:
    #opening the input file
    fout = open(inputFile, "w")

    #writing value of N to it
    fout.write(str(q)+"\n")

    #closing the file
    fout.close()

    #defining Y
    Y = [[],[],[],[],[]]

    for i in range(times):
        #running the executable and getting output in output.txt
        a = os.system("./a.out < "+ inputFile + " > " + outputFile)

        #reading from output.txt
        fin = open(outputFile, "r")

        #list of strings from fin
        L = list(filter(None, fin.read().split('\n')))

        for i in range(5):
            L[i] = float(L[i])
            Y[i].append(L[i])

    for i in range(5):
        Y[i] = sum(Y[i])/len(Y[i])

    Vals.append(np.array(Y))
Vals = np.array(Vals)
Vals = np.transpose(Vals)

# data to plot
n_groups = 3
means = Vals

# create plot
plt.figure(num=None, figsize=(12, 9), dpi=80, facecolor='w', edgecolor='k')
index = np.arange(n_groups)
bar_width = 0.15
opacity = 0.9

labels=['Non-preemptive FCFS',
            'Non-preemptive SJF',
            'Pre-emptive SJF',
            'Round Robin (δ = 2)',
            'Highest response-ratio']
colors=['#A0569E','#FFC99B','#3D4CAF','#49D89A','#E87F7F'] 
    
for i in range(5):
    plt.bar(index + bar_width*i, means[i], bar_width, alpha = opacity, color=colors[i],label=labels[i])
    
plt.xlabel('Value of N', fontsize=14)
plt.ylabel('Average Turnaround Times',fontsize=14)
plt.title('Comparison of different scheduling techniques',fontsize=14)
plt.xticks(index + 2*bar_width, ('N = 10', 'N = 50', 'N = 100'), fontsize=12)
temp = int(max(means.flatten()))+50
plt.yticks(ticks=range(0,temp,50),fontsize=12)
plt.legend(prop={'size': 15})

plt.tight_layout()
plt.savefig("barChart.png")