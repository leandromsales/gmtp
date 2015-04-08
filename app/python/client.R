library("outliers") #Para remover dados discrepantes
#elapsed <- rm.outlier(x=elapsed) #Example

client <- read.table("logclient.log", header=T, sep='\t') # Carrega o arquivo
ratelabel <- "Tx Rate (bytes/s)"

print("==================== Size of Packets ======================")
size <- client[3]

summary(size)
subset(table(size[,1]), table(size[,1]) == max(table(size[,1]))) # Moda
quantile(size[,1], seq(0.10, 0.9, 0.1))
var(size[,1]) # Variância
sd(size[,1])  # Desvio padrão
sd(size[,1])/mean(size[,1]) #  coef. de  variacao
#hist(size[,1], nclass=10, xlab="Size of packet (bytes)")
#barplot(size[,1], xlab="Time", ylab="Size of packet (s)")

print("==================== Instant Tx Rate ======================")
inst_rate <- client[5]
inst_rate <- rm.outlier(x=inst_rate)

summary(inst_rate)
quantile(inst_rate[,1], seq(0.10, 0.9, 0.1))
var(inst_rate[,1]) # Variância
sd(inst_rate[,1])  # Desvio padrão
sd(inst_rate[,1])/mean(inst_rate[,1]) # coef. de  variacao
hist(inst_rate[,1], nclass=20, main="Instant rate", xlab=ratelabel)
barplot(inst_rate[,1], main="Instant rate", xlab="Time", ylab=ratelabel)

print("==================== Last 1000 Tx Rate ======================")
rate1000 <- subset(client, rate1000>0)
rate1000 <- rate1000[8]
rate1000 <- rm.outlier(x=rate1000)

summary(rate1000)
quantile(rate1000[,1], seq(0.10, 0.9, 0.1))
var(rate1000[,1]) # Variância
sd(rate1000[,1])  # Desvio padrão
sd(rate1000[,1])/mean(rate1000[,1]) # coef. de  variacao
hist(rate1000[,1], nclass=40, main="Rate of groups of 1000 packets", xlab=ratelabel)
barplot(rate1000[,1], main="Rate of group of 1000 packets", xlab="Time", ylab=ratelabel)

print("==================== Total Tx Rate ======================")
rate <- client[11]
rate <- rm.outlier(x=rate)

summary(rate)
subset(table(rate[,1]), table(rate[,1]) == max(table(rate[,1]))) # Moda
quantile(rate[,1], seq(0.10, 0.9, 0.1))
var(rate[,1]) # Variância
sd(rate[,1])  # Desvio padrão
sd(rate[,1])/mean(rate[,1]) # coef. de  variacao
hist(rate[,1], nclass=40, main="Total rate", xlab=ratelabel)
barplot(rate[,1], main="Total rate", xlab="Time", ylab=ratelabel)
