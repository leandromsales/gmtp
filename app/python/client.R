library("outliers") #Para remover dados discrepantes
#elapsed <- rm.outlier(x=elapsed) #Example

client <- read.table("logclient.log", header=T, sep='\t') # Carrega o arquivo
ratelabel <- "Rx Rate (bytes/s)"

print("==================== Size of Packets ======================")
size <- client[3]

print("==================== Last 1000 RX Rate ======================")
rate1000 <- subset(client, rate1000>0)
rate1000 <- rate1000[8]
rate1000 <- rm.outlier(x=rate1000)

summary(rate1000)
var(rate1000[,1]) # Variância
sd(rate1000[,1])  # Desvio padrão
sd(rate1000[,1])/mean(rate1000[,1]) # coef. de  variacao
hist(rate1000[,1], nclass=40, main="Rx rate of groups of 1000 packets", xlab=ratelabel)
barplot(rate1000[,1], main="Rx rate of group of 1000 packets", xlab="Time", ylab=ratelabel)

print("==================== Total RX Rate ======================")
rate <- client[11]
rate <- rm.outlier(x=rate)

summary(rate)
subset(table(rate[,1]), table(rate[,1]) == max(table(rate[,1]))) # Moda
var(rate[,1]) # Variância
sd(rate[,1])  # Desvio padrão
sd(rate[,1])/mean(rate[,1]) # coef. de  variacao
hist(rate[,1], nclass=40, main="Total Rx rate", xlab=ratelabel)
barplot(rate[,1], main="Total Rx rate", xlab="Time", ylab=ratelabel)

