import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

df1 = pd.read_csv('graphData-10-128-0-23.csv' , skiprows=1)

#df = pd.read_csv('graphData-10-128-0-23.csv' , usecols = [i for i in range(n)])
for index, row in df1.iterrows(): #for each burst create a plot

    plt.xlabel(xlabel = 'Message # for Bust '+str(row.astype(np.int64)[1]))

    for i in range(0,row.astype(np.int64)[0]): #for each sucessful message plot two points
        #plot <pair #, o_i> in red, <pair #, d_i> in blue

        plt.scatter(i+1 , y =row[2*(i+1)], color='red')
        plt.scatter(i+1 , y =row[2*(i+1)+1], color='blue', label = 'delay', linewidths=0.5)
    plt.hlines(y=row[2*(row.astype(np.int64)[0])+1], xmin=0, xmax=row.astype(np.int64)[0], colors='blue', linestyles='dashed', lw=0.5, label='min delay')
    plt.hlines(y=row[2*(row.astype(np.int64)[0])], xmin=0, xmax=row.astype(np.int64)[0], colors='red', linestyles='dashed', lw=0.5, label='min delay')
    #plt.pyplot.plot()
    #for i in range(row.astype(np.int64)[0]): #for each message(sorted by send time) in burst plot x
    plt.show()




df2 = pd.read_csv('graphData-34-27-33-102.csv', skiprows=1)

df3 = pd.read_csv('graphData-132-163-96-1.csv', skiprows=1)
