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
  plot(colunm, type="n", main=mainlabel, xlab="Pacotes Recebidos", ylab=datalabel)
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

get_mean_table <- function(table){
  new_table <- c(ncol(table))
  for(i in 2:ncol(table)-1) {
    new_table[i-1] <- mean(table[,i])
  }
  return (new_table);
}

last_line <- function(table) {
  new_table <- c(ncol(table))
  last_row = nrow(table)
  for(i in 2:ncol(table)-1) {
    new_table[i-1] <- table[last_row, i]
  }
  return (new_table);
}

## ============= Tamanho amostral ============

# Corrigir (não pegar a média por tempo, mas a média consolidada por nó);
# Fazer um vetor de médias. Verificar como foi feito em Loss.

getz <- function(erro) {
  z <- (1-(erro/2)) * 2
  return (z)
}

getnumber <- function(populacao, media, erro, dp) {
  z <- getz(erro)
  ni <- ((z^2) * (dp^2)) / (erro^2)
  n <- (populacao * ni) / (populacao + ni - 1)
  return (ceiling(n));
}

getn <- function(pop, err) {
  return (getnumber(length(pop), mean(pop), err, sd(pop)));
}

## ============== START ===========
print("======= Starting ========")
main_label <- "GMTP"
gmtp_logs <- Sys.glob("~/gmtp/app/ns-3-dce/results/gmtp/gmtp-*.log")
gmtp_len <- length(gmtp_logs)

gmtp <- table_from_files(gmtp_logs, "idx")
err <- 0.05;

seq_gmtp <- sub_table(gmtp, 2, "idx")
loss_gmtp <- sub_table(gmtp, 3, "idx")
elapsed_gmtp <- sub_table(gmtp, 4, "idx")
rate_gmtp <- sub_table(gmtp, 6, "idx")
inst_rate_gmtp <- sub_table(gmtp, 7, "idx")
inst_rate_gmtp <- na.omit(inst_rate_gmtp)

## ============== LOSSES ===========
report(seq_gmtp$mean)
plot(seq_gmtp$mean, type="n", main="GMTP - Número de Sequencia", xlab="Pacotes Recebidos", ylab="Número de Sequencia")
lines(seq_gmtp$mean, lwd=3)
lines(gmtp$idx, col="red", lwd=2)

report(loss_gmtp$mean)
n <- 0
tot_loss <- c()
for(i in 2:ncol(loss_gmtp)-1) {
  loss <- sum(loss_gmtp[i]) / seq_gmtp[nrow(seq_gmtp), i]
  tot_loss[n] <- loss
  n <- n + 1
}
report(tot_loss)
loss_rate <- mean(tot_loss)*100
lg <- tot_loss
#lg <- get_mean_table(loss_gmtp);
#report(lg)

report(elapsed_gmtp$mean)
plot(elapsed_gmtp$mean, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Pacotes Recebidos", ylab="Intervalo entre dois pacotes (ms)")
points(elapsed_gmtp$mean)
lines(lowess(elapsed_gmtp$mean), col="yellow", lwd=3)
abline(lm(elapsed_gmtp$mean~gmtp$idx), col="green", lwd=3)

## ============== RX RATE ===========
rate_gmtp$mean[nrow(rate_gmtp)]

rg <- last_line(rate_gmtp);
report(rg)

report(inst_rate_gmtp$mean)
plot_graph(inst_rate_gmtp$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg <- get_mean_table(inst_rate_gmtp)
report(irg)

getn(lg*100, err)
getn(rg, err)
getn(irg, err)
