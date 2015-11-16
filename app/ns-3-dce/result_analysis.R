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

percent <- function(val, total) {
  ret = 0
  if(val > 0 && total > 0)
    ret = (val*100)/total
  else
    ret = 0;
  return(ret);
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

get_seq <- function(col, len){
  myseq <- seq(from = col, to = (len), by = 6)
  return(myseq)
}

sub_table <- function(table, col, key){
  sprintf("col: %d, ncol: %d", col, ncol(table))
  print (get_seq(col, ncol(table)));
  new_table <- table[c(1, get_seq(col, ncol(table)))]
   data_cols <- c(2, ncol(new_table))
   m <- data.frame(idx=new_table[,1], mean=NA)
   n <- new_table[,1];
   for(i in 1:nrow(new_table)) {
     tmp <- new_table[i, data_cols]
     m[i,2] <- (sum(tmp)/ncol(tmp))
   }
   new_table <- merge(new_table, m, by=key)
  
  return(new_table)
}

## ============== START ===========
print("======= Starting ========")
gmtp_logs <- Sys.glob("results/gmtp-*.log")
gmtp_len <- length(gmtp_logs)

gmtp <- table_from_files(gmtp_logs, "idx")

seq_gmtp <- sub_table(gmtp, 2, "idx")
loss_gmtp <- sub_table(gmtp, 3, "idx")
elapsed_gmtp <- sub_table(gmtp, 4, "idx")
rate_gmtp <- sub_table(gmtp, 6, "idx")
inst_rate_gmtp <- sub_table(gmtp, 7, "idx")

main_label <- "GMTP"

## ============== LOSSES ===========
report(seq_gmtp$mean)
plot(seq_gmtp$mean, type="n", main="GMTP - Número de Sequencia", xlab="Tempo", ylab="Número de Sequencia")
lines(seq_gmtp$mean, lwd=3)
lines(gmtp$idx, col="red", lwd=2)

report(loss_gmtp$mean)
plot(loss_gmtp$mean, type="n", main="GMTP - Perdas", xlab="Tempo", ylab="Número de Perdas")
lines(loss_gmtp$mean, lwd=3)

#pkts_sent <- gmtp$seq[length(gmtp$seq)]  - gmtp$seq[1]
#pkts_rcv <- length(gmtp$idx)
#losses01 <- pkts_sent - pkts_rcv
#perc_losses01 = percent(losses01, pkts_sent)
# 
#losses <- sum(loss_gmtp$mean)
#perc_losses = percent(losses, pkts_sent)
#cat("Losses: "); print(perc_losses); cat("%");

report(elapsed_gmtp$mean)
plot(elapsed_gmtp$mean, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Tempo", ylab="Intervalo entre dois pacotes (ms)")
points(elapsed_gmtp$mean)
lines(lowess(elapsed_gmtp$mean), col="yellow", lwd=3)
abline(lm(elapsed_gmtp$mean~gmtp$idx), col="green", lwd=3)

## ============== RATES ===========
report(rate_gmtp$mean)
plot_graph(rate_gmtp$mean, "GMTP - Taxa de Recepção Total", "Taxa de Recepção Total (B/s)")
# lines(rate_gmtp$rx_rate.x, col="red")
# lines(rate_gmtp$rx_rate.y, col="green")
# lines(rate_gmtp$rx_rate, col="blue")

report(inst_rate_gmtp$mean)
plot_graph(inst_rate_gmtp$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
# lines(inst_rate_gmtp$inst_rx_rate.x, col="red")
# lines(inst_rate_gmtp$inst_rx_rate.y, col="green")
# lines(inst_rate_gmtp$inst_rx_rate, col="blue")


