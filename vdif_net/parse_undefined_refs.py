import sys
for l in sys.stdin:
   where = l.find("undefined reference to")
   if where > -1:
       print(l[where+len("undefined reference to")+2:-2])
