## ========================= FUNCTIONS ========================

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

## ================ Geral ===============================
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
  #sprintf("col: %d, ncol: %d", col, ncol(table))
  #print (seq(col, ncol(table), by));
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

## =================== AUX ======================
poisson <- function(x, y, n) {
  mx <- mean(x)
  my <- mean(y)
  r <- n * (log(my*100)/mx) 
  return (exp(r/100) - 1);  # e^n
}

project <- function(x, y, n, max=0) {
  ajuste <- lm(formula = y~log(x))
  #summary(ajuste)
  #anova(ajuste)
  #confint(ajuste)
  if(max>0) {
    plot(range(c(0, 31)), range(c(0, max)))
    #abline(x, exp(y))
    abline(lm(y~x))
    points(y~x)
  } 
  #predict(ajuste,x0,interval="confidence")
  ret <- predict(ajuste, data.frame(x=n), interval="prediction")
  return (ret)
}
## ================= Loss ==========================
print_seq_graph <- function(seq, idx) {
  plot(seq, type="n", main="GMTP - Número de Sequencia", xlab="Pacotes Recebidos", ylab="Número de Sequencia")
  lines(seq)
  lines(idx, col="red")
}

losses <- function(loss_table, seq) {
  n <- 0
  losses <- c()
  for(i in 2:ncol(loss_table)-1) {
    loss <- sum(loss_table[i]) / seq[nrow(seq), i]
    losses[n] <- loss
    n <- n + 1
  }
  return (losses)
}

loss_rate <- function(losses) {
  return (mean(losses)*100)
}

## ============== Continuidade =====================

print_elapsed <- function(elapsed, idx) {
  plot(elapsed, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Pacotes Recebidos", ylab="Intervalo entre dois pacotes (ms)")
  points(elapsed)
  lines(lowess(elapsed), col="yellow")
  abline(lm(elapsed~idx), col="green")
}

continuidade <- function(tabela, media, coluna, loss) {
  late <- subset(x = tabela, subset = mean > media, select = coluna)
  contin <- 100 - (nrow(late)*100/nrow(tabela)) - loss
  return (contin)
}

##======================== NDP ===================

ndp_clientn <- function(ndp_clients, ndp_server, elapsed_sum) {
  ndpc <- last_line(ndp_clients)
  ndps <- last_line(ndp_server)
  n <- length(ndps);
  
  ret <- sum(ndpc)/n
  ret <- ret + 2*sum(elapsed_sum)/1000
  return (ceiling(ret));
}

ndp_servern <- function(ndp_server) {
  ndps <- last_line(ndp_server)
  return (ceiling(mean(ndps)))
}

ndp2 <- function(ndpc, ndps) {
  return (ceiling(ndpc + ndps));
}

ndp <- function(ndp_clients, ndp_server, elapsed_sum) {
  ndpc <- ndp_clientn(ndp_clients, ndp_server, elapsed_sum)
  ndps <- ndp_servern(ndp_server)
  return (ndp2(ndpc, ndps));
}

ndp_len <- function(ndp) {
  return (ndp * (36+36)/1000)
}

ndp_quantile <- function(len) {
  return (len * 1000/(36+36))
}

norm.interval = function(data, variance = var(data), conf.level = 0.95) {
  z = qnorm((1 - conf.level)/2, lower.tail = FALSE) 
  xbar = mean(data)
  sdx = sqrt(variance/length(data))
  c(xbar - z * sdx, xbar + z * sdx)
}


## ======================== Simulation =======================

main_label <- "GMTP"
err <- 0.05;




