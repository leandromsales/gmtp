## ========================= FUNCTIONS ========================

source("master.R");

## ============== START ===========
print("======= Starting ========")

log_dir06 <- "~/gmtp/app/ns-3-dce/results/sim-6"
client_files06 <- paste(log_dir06, "/client-*.log", sep = "")
server_files06 <- paste(log_dir06, "/server-*.log", sep = "")

clients_logs06 <- Sys.glob(client_files06)
clients_len06 <- length(clients_logs06)
clients06 <- table_from_files(clients_logs06, "idx")

seq_gmtp06 <- sub_table(clients06, 2, "idx", 7)
loss_gmtp06 <- sub_table(clients06, 3, "idx", 7)
elapsed_gmtp06 <- sub_table(clients06, 4, "idx", 7)
rate_gmtp06 <- sub_table(clients06, 6, "idx", 7)
inst_rate_gmtp06 <- sub_table(clients06, 7, "idx", 7)
inst_rate_gmtp06 <- na.omit(inst_rate_gmtp06)
ndp_clients06 <- sub_table(clients06, 8, "idx", 7)

server_logs06 <- Sys.glob(server_files06)
server_len06 <- length(server_logs06)
server06 <- table_from_files(server_logs06, "idx")

ndp_server06 <- sub_table(server06, 2, "idx", 1)

## ============== LOSSES ===========
report(seq_gmtp06$mean)
plot(seq_gmtp06$mean, type="n", main="GMTP - Número de Sequencia", xlab="Pacotes Recebidos", ylab="Número de Sequencia")
lines(seq_gmtp06$mean, lwd=3)
lines(clients06$idx, col="red", lwd=2)

report(loss_gmtp06$mean)
n <- 0
tot_loss06 <- c()
for(i in 2:ncol(loss_gmtp06)-1) {
  loss <- sum(loss_gmtp06[i]) / seq_gmtp06[nrow(seq_gmtp06), i]
  tot_loss06[n] <- loss
  n <- n + 1
}
report(tot_loss06)
loss_rate06 <- mean(tot_loss06)*100
lg06 <- tot_loss06
#lg <- get_mean_table(loss_gmtp);
#report(lg)

report(elapsed_gmtp06$mean)
plot(elapsed_gmtp06$mean, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Pacotes Recebidos", ylab="Intervalo entre dois pacotes (ms)")
points(elapsed_gmtp06$mean)
lines(lowess(elapsed_gmtp06$mean), col="yellow", lwd=3)
abline(lm(elapsed_gmtp06$mean~clients06$idx), col="green", lwd=3)

## ============== RX RATE ===========
rate_gmtp06$mean[nrow(rate_gmtp06)]

rg06 <- last_line(rate_gmtp06);
report(rg06)

report(inst_rate_gmtp06$mean)
plot_graph(inst_rate_gmtp06$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg06 <- get_mean_table(inst_rate_gmtp06)
report(irg06)

ndpc06 <-last_line(ndp_clients06);
report(ndpc06)
ndps06 <-last_line(ndp_server06);
report(ndps06)

c_ndp06 <- ceiling(ndp_clients06$mean[nrow(ndp_clients06)] + 2 * sum(elapsed_gmtp06$mean)/1000)
s_ndp06 <- ceiling(ndp_server06$mean[nrow(ndp_server06)])
ndp06 <- c_ndp06 + s_ndp06

