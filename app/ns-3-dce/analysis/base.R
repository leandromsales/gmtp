## ========================= FUNCTIONS ========================

source("~/gmtp/app/ns-3-dce/analysis/master.R");

## ============== START ===========
print("======= Starting ========")

pre <- TRUE

log_dir <- "~/gmtp/app/ns-3-dce/results/gmtp"
client_files <- paste(log_dir, "/client-*.log", sep = "")
server_files <- paste(log_dir, "/server-*.log", sep = "")

clients_logs <- Sys.glob(client_files)
clients_len <- length(clients_logs)
clients <- table_from_files(clients_logs, "idx")

seq_gmtp <- sub_table(clients, 2, "idx", 7)
loss_gmtp <- sub_table(clients, 3, "idx", 7)
elapsed_gmtp <- sub_table(clients, 4, "idx", 7)
rate_gmtp <- sub_table(clients, 6, "idx", 7)
inst_rate_gmtp <- sub_table(clients, 7, "idx", 7)
inst_rate_gmtp <- na.omit(inst_rate_gmtp)
ndp_clients <- sub_table(clients, 8, "idx", 7)

server_logs <- Sys.glob(server_files)
server_len <- length(server_logs)
server <- table_from_files(server_logs, "idx")

ndp_server <- sub_table(server, 2, "idx", 1)

## ============== LOSSES ===========
report(seq_gmtp$mean)
plot(seq_gmtp$mean, type="n", main="GMTP - Número de Sequencia", xlab="Pacotes Recebidos", ylab="Número de Sequencia")
lines(seq_gmtp$mean, lwd=3)
lines(clients$idx, col="red", lwd=2)

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
abline(lm(elapsed_gmtp$mean~clients$idx), col="green", lwd=3)

## ============== CONTINUIDADE ===========
contin <- continuidade(elapsed_gmtp, 7, "mean", loss_rate)

## ============== RX RATE ===========
rate_gmtp$mean[nrow(rate_gmtp)]

rg <- last_line(rate_gmtp);
report(rg)

report(inst_rate_gmtp$mean)
plot_graph(inst_rate_gmtp$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg <- get_mean_table(inst_rate_gmtp)
report(irg)

ndpc <-last_line(ndp_clients);
report(ndpc)
ndps <-last_line(ndp_server);
report(ndps)

c_ndp <- ceiling(ndp_clients$mean[nrow(ndp_clients)] + 2 * sum(elapsed_gmtp$mean)/1000)
s_ndp <- ceiling(ndp_server$mean[nrow(ndp_server)])
ndp <- c_ndp + s_ndp

if(pre) {
  print (getn(lg*100, err, 10))
  print (getn(rg, err, 10))
  print (getn(irg, err, 10))
  print( getn(ndpc, err, 10))
  print( getn(ndps, err, 10))
}



