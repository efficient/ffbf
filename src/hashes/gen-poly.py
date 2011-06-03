#!/usr/bin/python
# Jan's script to generate a random irreducible polynomial for rabin

def gcd(x, y):
   while 1:
	if y == 0: return x
	x = x % y
	if x == 0: return y
	y = y % x

def degree(f):
   i = 0
   while f:
       f = f / 2
       i = i + 1
   return i

def isreducible(f):
   m = degree(f) / 2
   u = 2
   for i in range(m):
	u = (u * u) % f
	if gcd(f, u ^ 2) != 1:
	    return True
   return False

def gen_poly(degree):
   import random
   f = random.getrandbits(degree)
   return f | (1 << (degree - 1))

while 1:
   p = gen_poly(64)
   if not isreducible(p):
	print hex(p)
	break

