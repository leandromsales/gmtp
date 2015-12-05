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
    abline(lm(colunm~clients$idx), col="green", lwd=3)
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
  myseq <- seq(from = col, to = (len), by = 7)
  return(myseq)
}

sub_table <- function(table, col, key, by){
  sprintf("col: %d, ncol: %d", col, ncol(table))
  print (seq(col, ncol(table), by));
  new_table <- table[c(1, seq(col, ncol(table), by))]
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

getn <- function(pop, err, rep) {
  return (getnumber(length(pop)/rep, mean(pop), err, sd(pop)));
}

main_label <- "GMTP"
err <- 0.05;


