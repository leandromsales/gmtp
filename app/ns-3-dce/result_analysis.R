## ========================= FUNCTIONS ========================
table_from_file <- function(filename) {
  return (read.table(toString(filename), header=T, sep='\t'))
}

ratelabel <- "Taxa de Recepção (bytes/s)"
print_graphs <- function(col, main_label){
  hist(col, nclass=40, main=main_label, xlab=ratelabel)
  #barplot(col, main=main_label, xlab="Tempo", ylab=ratelabel)
  plot(col, type="n", main=main_label, xlab="Tempo", ylab=ratelabel)
  lines(col)
}

report <- function(col) {
  cat("\n")
  print(summary(col))
  cat("\nVariância: "); print(var(col)) 
  cat("Desvio Padrão: "); print(sd(col)) 
  cat("Coef. de Variação: "); print(sd(col)/mean(col))
}

## ============== START ===========
print("======= Starting ========")
gmtp_log <- Sys.glob("file.log")
gmtp_len <- length(gmtp_log)

#gmtp <- table_from_file("file.log")
gmtp <- read.table("file.log", header=T, sep='\t')

rate_gmtp = gmtp$rx_rate
inst_rate_gmtp =  gmtp$inst_rx_rate

report(gmtp$elapsed)
report(rate_gmtp)
report(inst_rate_gmtp)

main_label <- "GMTP"

hist(gmtp$elapsed, nclass=40, main=main_label, xlab="Time between two packtes")
plot(gmtp$elapsed, type="n", main="GMTP", xlab="Tempo", ylab="Time between two packtes")
lines(gmtp$elapsed)

hist(rate_gmtp, nclass=40, main=main_label, xlab="RX rate")
plot(rate_gmtp, type="n", main="GMTP", xlab="Tempo", ylab="RX rate")
lines(rate_gmtp)

hist(inst_rate_gmtp, nclass=40, main=main_label, xlab="Inst RX rate")
plot(inst_rate_gmtp, type="n", main="GMTP", xlab="Tempo", ylab="Inst RX rate")
lines(inst_rate_gmtp)

pkts_sent <- gmtp$seq[length(gmtp$seq)]  - gmtp$seq[1]
pkts_rcv <- length(gmtp$idx)
losses <- pkts_sent - pkts_rcv
perc_losses = losses*100/pkts_sent

