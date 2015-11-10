## ========================= FUNCTIONS ========================
table_from_file <- function(filename) {
  return (read.table(toString(filename), header=T, sep='\t'))
}

plot_graph <- function(colunm, mainlabel, datalabel, extra=0){
  #hist(colunm, nclass=40, main=mainlabel, xlab=datalabel)
  plot(colunm, type="n", main=mainlabel, xlab="Tempo", ylab=datalabel)
  lines(colunm)
  if(extra) {
    lines(lowess(colunm), col="yellow", lwd=3)
    abline(lm(colunm~gmtp$idx), col="green", lwd=3)
  }
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
gmtp_log <- Sys.glob("stdout")
gmtp_len <- length(gmtp_log)

#gmtp <- table_from_file("file.log")
gmtp <- read.table("/home/wendell/gmtp/app/ns-3-dce/stderr", header=T, sep='\t')

rate_gmtp = gmtp$rx_rate
inst_rate_gmtp =  gmtp$inst_rx_rate

main_label <- "GMTP"

report(gmtp$seq)
plot(gmtp$seq, type="n", main="GMTP - Número de Sequencia", xlab="Tempo", ylab="Número de Sequencia")
lines(gmtp$seq, lwd=3)
lines(gmtp$idx, col="red", lwd=2)

report(gmtp$elapsed)
plot(gmtp$elapsed, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Tempo", ylab="Intervalo entre dois pacotes (ms)")
points(gmtp$elapsed)
lines(lowess(gmtp$elapsed), col="yellow", lwd=3)
abline(lm(gmtp$elapsed~gmtp$idx), col="green", lwd=3)

report(rate_gmtp)
plot_graph(rate_gmtp, "GMTP - Taxa de Recepção Total", "Taxa de Recepção Total (B/s)")

report(inst_rate_gmtp)
plot_graph(inst_rate_gmtp, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")

pkts_sent <- gmtp$seq[length(gmtp$seq)]  - gmtp$seq[1]
pkts_rcv <- length(gmtp$idx)
losses <- pkts_sent - pkts_rcv
perc_losses = losses*100/pkts_sent

