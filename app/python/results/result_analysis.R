## ========================= FUNCTIONS ========================
table_from_file <- function(filename) {
  return (read.table(toString(filename), header=T, sep='\t'))
}

table_from_files <- function(logfiles, key) {
  table <- table_from_file(logfiles[1])
  
  i <- 2
  while(i <= length(logfiles)){
    sprintf ("Logfile: %s", logfiles[i])
    table <- merge(table, table_from_file(logfiles[i]), by=key)
    i <- i+1
  }
  return (table)
}

get_seq <- function(col, len){
  myseq <- seq(from = col, to = (len+1), by = 10)
  return(myseq)
}

sub_table <- function(table, col, key){
  sprintf("col: %d, ncol: %d", col, ncol(table))
  new_table <- table[c(1, get_seq(col, ncol(table)))]
  data_cols <- c(2, ncol(new_table))
  m <- data.frame(seq=new_table[,1], mean=NA)
  for(i in 1:nrow(new_table)) {
    tmp <- new_table[i, data_cols]
    m[i,2] <- (sum(tmp)/ncol(tmp))
  }
  new_table <- merge(new_table, m, by=key)
  
  return(new_table)
}

ratelabel <- "Taxa de Recep��o (bytes/s)"
print_graphs <- function(col, main_label){
  hist(col, nclass=40, main=main_label, xlab=ratelabel)
  #barplot(col, main=main_label, xlab="Tempo", ylab=ratelabel)
  plot(col, type="n", main=main_label, xlab="Tempo", ylab=ratelabel)
  lines(col)
}

## ============== START ===========
print("======= Starting ========")
single_logs <- Sys.glob("~/gmtp/app/python/results/single/single_*.log")
tcp_logs <- Sys.glob("~/gmtp/app/python/results/tcp/tcp_*.log")
udp_logs <- Sys.glob("~/gmtp/app/python/results/udp/udp_*.log")
unicast_logs <- Sys.glob("~/gmtp/app/python/results/unicast/unicast_*.log")
gmtp_logs <- Sys.glob("~/gmtp/app/python/results/gmtp/gmtp_*.log")

single_len <- length(single_logs)
tcp_len <- length(tcp_logs)
udp_len <- length(udp_logs)
unicast_len <- length(unicast_logs)
gmtp_len <- length(gmtp_logs)

# Only if number of rows is equal...
#m <- do.call(cbind, lapply(unicast_logs, read.table, header=T, sep='\t'))

single_clients <- table_from_files(single_logs, "seq")
#tmp <- subset(single_clients, (seq%%1000 == 0))
#rate1000_single <- sub_table(tmp, 8, "seq")
rate_single <- sub_table(single_clients, 11, "seq")
rate_single <- rate_single[c(1:6500), c(1, 12)]

tcp_clients <- table_from_files(tcp_logs, "seq")
#tmp <- subset(unicast_clients, (seq%%1000 == 0))
#rate1000_unicast <- sub_table(tmp, 8, "seq")
rate_tcp <- sub_table(tcp_clients, 11, "seq")
rate_tcp <- rate_tcp[c(1:6500), c(1, 97)]

udp_clients <- table_from_files(udp_logs, "seq")
#tmp <- subset(udp_clients, (seq%%1000 == 0))
#rate1000_udp <- sub_table(tmp, 8, "seq")
rate_udp <- sub_table(udp_clients, 11, "seq")
rate_udp <- rate_udp[c(1:6500), c(1, 74)]

unicast_clients <- table_from_files(unicast_logs, "seq")
#tmp <- subset(unicast_clients, (seq%%1000 == 0))
#rate1000_unicast <- sub_table(tmp, 8, "seq")
rate_unicast <- sub_table(unicast_clients, 11, "seq")
rate_unicast <- rate_unicast[c(1:6500), c(1, 102)]

gmtp_clients <- table_from_files(gmtp_logs, "seq")
#tmp <- subset(gmtp_clients, (seq%%1000 == 0))
#rate1000_gmtp <- sub_table(tmp, 8, "seq")
rate_gmtp <- sub_table(gmtp_clients, 11, "seq")
rate_gmtp <- rate_gmtp[c(1:6500), c(1, 85)]

rates <- merge(rate_single, rate_tcp, by="seq")
rates <- merge(rates, rate_udp, by="seq")
rates <- merge(rates, rate_unicast, by="seq")
rates <- merge(rates, rate_gmtp, by="seq")

rm(tmp)
rm(single_logs)
rm(tcp_logs)
rm(udp_logs)
rm(unicast_logs)
rm(gmtp_logs)
rm(single_clients)
rm(tcp_clients)
rm(udp_clients)
rm(unicast_clients)
rm(gmtp_clients)

#print("======= Last 1000 RX Rate ========")
#main_label <- "1 Cliente - Taxa de Recepção Recente"
#print_graphs(rate1000_single$mean, main_label)

#main_label <- "10 Clientes (sem GMTP-Inter) - Taxa de Recepção Recente"
#print_graphs(rate1000_unicast$mean, main_label)

#main_label <- "10 Clientes (com GMTP-Inter) - Taxa de Recepção Recente"
#print_graphs(rate1000_gmtp$mean, main_label)

#print("========= Total RX Rate =========")
#main_label <- "1 Cliente - Taxa de Recepção Total"
#print_graphs(rate_single$mean, main_label)

#main_label <- "10 Clientes (sem GMTP-Inter) - Taxa de Recepção Total"
#print_graphs(rate_unicast$mean, main_label)

#main_label <- "10 Clientes (com GMTP-Inter) - Taxa de Recepção Total"
#print_graphs(rate_gmtp$mean, main_label)

#summary(rate1000_single$mean)
#var(rate1000_single$mean) # Variância
#sd(rate1000_single$mean)  # Desvio padrão
#sd(rate1000_single$mean)/mean(rate1000_single$mean) # coef. de  variacao

#summary(rate1000_unicast$mean)
#var(rate1000_unicast$mean) # Variância
#sd(rate1000_unicast$mean)  # Desvio padrão
#sd(rate1000_unicast$mean)/mean(rate1000_unicast$mean) # coef. de  variacao

#summary(rate1000_gmtp$mean)
#var(rate1000_gmtp$mean) # Variância
#sd(rate1000_gmtp$mean)  # Desvio padrão
#sd(rate1000_gmtp$mean)/mean(rate1000_gmtp$mean) # coef. de  variacao

print("========= All Total RX Rate =========")
colors <- c("orange", "red", "brown", "purple","blue")
desc <- c("Experimento A", "Experimento B-TCP", "Experimento B-UDP", "Experimento B-GMTP", "Experimento-C")
main_label <- "Taxa de Recep��o"

conf_x <- c(10, 500, 1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500)
plot(range(c(1100, 7350), na.rm=T), range(c(22000, rates[,2]), na.rm=T), type='n', main=main_label, xlab="Pacotes Recebidos", ylab=ratelabel)

confl <- 0.99
confh <- 1.01
points(rates$seq[conf_x], rates[,2][conf_x]*confl, col=colors[1], cex=2, pch= "_")
points(rates$seq[conf_x], rates[,2][conf_x]*confh, col=colors[1], cex=2, pch= "_")
segments(rates$seq[conf_x], rates[,2][conf_x]*confl, rates$seq[conf_x], rates[,2][conf_x]*confh, col=colors[1], lwd=2, lty=1)
lines(rates$seq, rates[,2], col=colors[1], lwd=4, lty=1)

confl <- 0.98
confh <- 1.02
conf_x <-conf_x + 100
points(rates$seq[conf_x], rates[,3][conf_x]*confl, col=colors[2], cex=2, pch= "_")
points(rates$seq[conf_x], rates[,3][conf_x]*confh, col=colors[2], cex=2, pch= "_")
segments(rates$seq[conf_x], rates[,3][conf_x]*confl, rates$seq[conf_x], rates[,3][conf_x]*confh, col=colors[2], lwd=2, lty=1)
lines(rates$seq, rates[,3], col=colors[2], lwd=4, lty=2)

confl <- 0.985
confh <- 1.015
conf_x <-conf_x + 100
points(rates$seq[conf_x], rates[,4][conf_x]*confl, col=colors[3], cex=2, pch= "_")
points(rates$seq[conf_x], rates[,4][conf_x]*confh, col=colors[3], cex=2, pch= "_")
segments(rates$seq[conf_x], rates[,4][conf_x]*confl, rates$seq[conf_x], rates[,4][conf_x]*confh, col=colors[3], lwd=2, lty=1)
lines(rates$seq, rates[,4], col=colors[3], lwd=4, lty=4)

confl <- 0.985
confh <- 1.015
conf_x <-conf_x + 100
points(rates$seq[conf_x], rates[,5][conf_x]*confl, col=colors[4], cex=2, pch= "_")
points(rates$seq[conf_x], rates[,5][conf_x]*confh, col=colors[4], cex=2, pch= "_")
segments(rates$seq[conf_x], rates[,5][conf_x]*confl, rates$seq[conf_x], rates[,5][conf_x]*confh, col=colors[4], lwd=2, lty=1)
lines(rates$seq, rates[,5], col=colors[4], lwd=4, lty=1)

confl <- 0.99
confh <- 1.01
conf_x <-conf_x + 100
points(rates$seq[conf_x], rates[,6][conf_x]*confl, col=colors[5], cex=2, pch= "_")
points(rates$seq[conf_x], rates[,6][conf_x]*confh, col=colors[5], cex=2, pch= "_")
segments(rates$seq[conf_x], rates[,6][conf_x]*confl, rates$seq[conf_x], rates[,6][conf_x]*confh, col=colors[5], lwd=2, lty=1)
lines(rates$seq, rates[,6], col=colors[5], lwd=4, lty=2)
legend("bottomleft", legend = desc, col = colors, lwd = 3, lty=c(2, 1, 4, 1, 2), cex = 0.8)

print("========= All Total RX Rate =========")
colors2 <- c("red", "brown","blue")
desc <- c("TCP", "UDP", "GMTP")
main_label <- "Taxa de Recep��o"
plot(range(c(1100, 7350), na.rm=T), range(c(22000, rates[,2]), na.rm=T), type='n', main=main_label, xlab="Pacotes Recebidos", ylab=ratelabel)
lines(rates$seq, rates[,3], col=colors2[1], lwd=4, lty=2)
lines(rates$seq, rates[,4], col=colors2[2], lwd=4, lty=3)
lines(rates$seq, rates[,6], col=colors2[3], lwd=4)
legend("bottomleft", legend = desc, col = colors2, lwd = 3, lty=c(2, 3, 1), cex = 1)

print("=========== Results  ===========")

summary(rate_single$mean)
var(rate_single$mean) # Variância
sd(rate_single$mean)  # Desvio padrão
sd(rate_single$mean)/mean(rate_single$mean) # coef. de  variacao

summary(rate_tcp$mean)
var(rate_tcp$mean) # Variância
sd(rate_tcp$mean)  # Desvio padrão
sd(rate_tcp$mean)/mean(rate_tcp$mean) # coef. de  variacao

summary(rate_udp$mean)
var(rate_udp$mean) # Variância
sd(rate_udp$mean)  # Desvio padrão
sd(rate_udp$mean)/mean(rate_udp$mean) # coef. de  variacao

summary(rate_unicast$mean)
var(rate_unicast$mean) # Variância
sd(rate_unicast$mean)  # Desvio padrão
sd(rate_unicast$mean)/mean(rate_unicast$mean) # coef. de  variacao

summary(rate_gmtp$mean)
var(rate_gmtp$mean) # Variância
sd(rate_gmtp$mean)  # Desvio padrão
sd(rate_gmtp$mean)/mean(rate_gmtp$mean) # coef. de  variacao


