
import re

def filter(tab):
    res = []
    for row in tab:
        if row[0].find('usec') < 0:
            res.append(row)
    return res

def function_histogram(tab):
    histo = {}
    for row in tab:
        if row[0].find('usec') < 0:
            g = re.match('.*\s([a-zA-Z_][a-zA-Z_0-9]*\.[a-zA-Z_][a-zA-Z_0-9]*\().*;', row[0])
            if g:
                f = g.groups(1)[0][:-1] # Remove the trailing round bracket
                if f in histo:
                    histo[f]+=1
                else:
                    histo[f]=1
    nhisto = []
    for key,val in histo.items():
        row=[]
        row.append(key)
        row.append(str(val))
        nhisto.append(row)
    return nhisto
