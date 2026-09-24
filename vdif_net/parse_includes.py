import sys
for l in sys.stdin:
    if l[0] == "#":
        l = l.split()
        if l[1].isdigit():
            if l[2][0] == '"' and l[2][-1] == '"':
                fname = l[2][1:-1]
                print(fname)
