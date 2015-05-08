library("outliers") #Para remover dados discrepantes
#elapsed <- rm.outlier(x=elapsed) #Example

client_ideal <- read.table("logclient_ideal.log", header=T, sep='\t') # Carrega o arquivo
client_unicast <- read.table("logclient_unicast.log", header=T, sep='\t') # Carrega o arquivo
client_gmtp<- read.table("logclient_gmtp.log", header=T, sep='\t') # Carrega o arquivo

ratelabel <- "Rx Rate (bytes/s)"

print("==================== Last 1000 RX Rate ======================")
rate1000_ideal <- subset(client_ideal, rate1000>0)
rate1000_ideal <- rate1000_ideal[, c(1,8)]
summary(rate1000_ideal[,2])

rate1000_unicast <- subset(client_unicast, rate1000>0)
rate1000_unicast <- rate1000_unicast[, c(1,8)]
summary(rate1000_unicast[,2])

rate1000_gmtp <- subset(client_gmtp, rate1000>0)
rate1000_gmtp <- rate1000_gmtp[, c(1,8)]
summary(rate1000_gmtp[,2])

#hist(rate1000[,1], nclass=40, main="Rx rate of groups of 1000 packets", xlab=ratelabel)
plot(rate1000_ideal, rate1000_unicast, rate1000_gmtp, main="Rx rate of group of 1000 packets", xlab="Time", ylab=ratelabel, xlim=range(2000, 30000), ylim=range(0, 50000))
lines(rate1000_ideal, rate1000_unicast, rate1000_gmtp)

print("==================== Total RX Rate ======================")
rate_ideal <- client_ideal[, c(1,8)]
rate_ideal <- rm.outlier(x=rate_ideal)
summary(rate_ideal[,2])

rate_unicast <- client_unicast[, c(1,8)]
rate_unicast <- rm.outlier(x=rate_unicast)
summary(rate_unicast[,2])

rate_gmtp <- client_gmtp[, c(1,8)]
rate_gmtp <- rm.outlier(x=rate_gmtp)
summary(rate_gmtp[,2])


#hist(rate[,1], nclass=40, main="Total Rx rate", xlab=ratelabel)
#barplot(rate[,1], main="Total Rx rate", xlab="Time", ylab=ratelabel)
plot(rate_ideal, rate_unicast, rate_gmtp, main="Rx rate of group of 1000 packets", xlab="Time", ylab=ratelabel, xlim=range(1000, 8000), ylim=range(0, 50000))
lines(rate_ideal, rate_unicast, rate_gmtp)
