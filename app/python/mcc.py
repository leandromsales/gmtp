#!/usr/bin/python
# coding: utf-8
# GMTP-MCC tests

import sys
from cmath import sqrt

s = 203
R = 0.0001
p = 0.5

if(len(sys.argv) > 3):
    p = float(sys.argv[3])
if(len(sys.argv) > 2):
    R = float(sys.argv[2])/1000000
if(len(sys.argv) > 1):
    s = int(sys.argv[1])
    
#
#                             s
#  X_calc  =  -------------------------------------------------------
#             R * sqrt(p*2/3) + (12 * R * sqrt(p*3/8) * (p + 32*p^3))
#
# which we can break down into:
#
#                  s
#    X_calc  =  ---------
#                R * f(p)
#
# where f(p) is given for 0 < p <= 1 by:
#
#    f(p)  =  sqrt(2*p/3) + 12 * sqrt(3*p/8) *  (p + 32*p^3)
#

def fn(p):
    sr = sqrt(2*p/3)
    t = 12 * sqrt(3*p/8) * (p + 32*pow(p, 3))
    fp = sr + t
    print "sqrt(2*p/3) + 12 * sqrt(3*p/8) * (p + 32*pow(p, 3)) = f(p)"
    print sr, "+", t, " = ", fp, "\n"
    return fp

def mcc_calc_x(s, R, p):
    f = fn(p)
    q = R*f
    X = s/q
    print "s / R * f(p) = X"
    print s, "/", R, "*", f, "= X"
    print s, "/", q, "=", X, "\n"
    return X

x = mcc_calc_x(s, R, p)
print "New rate: ", x, "bytes/s\n"

