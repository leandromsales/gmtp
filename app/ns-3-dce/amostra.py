#!/usr/bin/python

import sys
from optparse import Option, OptionParser
from math import *

usage = "usage: %prog [options]."
parser = OptionParser(usage=usage, version="%prog 1.0")

parser.add_option("-n", "--nclients", dest="populacao",
                  help="O tamanho da populacao da amostra.", metavar="populacao")
                  
parser.add_option("-u", "--media", dest="media",
                  help="A media da variavel na amostra.", metavar="media")

parser.add_option("-d", "--desvio", dest="dp",
                  help="O desvio padrao da variavel na amostra.", metavar="dp")
                  
parser.add_option("-e", "--erro", dest="erro",
                  help="O erro maximo do experimento.", metavar="erro")
                
(options, args) = parser.parse_args()

media = 1
populacao = 1
erro = 0.05
dp = 1

option = False
if (options.populacao):
    populacao = int(options.populacao)
    option = True
if (options.media):
    media = float(options.media)
    option = True
if (options.dp):
    dp = float(options.dp)
    option = True
if (options.erro):
    erro = float(options.erro)
    option = True
if (option == False):
    parser.print_help()
    sys.exit(1)
    
def getz(erro):
    return (1-erro/2) * 2

def getnumber(populacao, media, erro, dp):
    z = getz(erro)
        
    ni = (pow(z, 2) * pow(dp, 2)) / pow(erro, 2)
    n = (populacao * ni) / (populacao + ni - 1)
    
    print "1 - erro: ", 1 - erro
    print "z: ", z
    print "z^2: ", pow(z, 2)
    print "dp: ", dp
    print "dp^2: ", pow(dp, 2)
    print "z^2 * dp^2: ", pow(z, 2) * pow(dp, 2)
    print "erro^2: ", pow(erro, 2)
    print "ni: ", ni
    print "populacao * ni: ", (populacao * ni)
    print "populacao + ni - 1: ", (populacao + ni - 1)
    print "n: ", n
    print "ceil(n): ", ceil(n)
    print ""
    
    return ceil(n)

def ic(populacao, media, erro, dp):
    factor = getz(erro) * (dp/sqrt(populacao))
    return (media - factor, media + factor)

print "Populacao: ", populacao
print "Media esperada: ", media
print "Erro maximo: ", erro
print " "

n = getnumber(populacao, media, erro, dp)
print ic(n, media, erro, dp)
print ic(populacao, media, erro, dp)
