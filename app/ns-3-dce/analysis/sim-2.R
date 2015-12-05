## ========================= FUNCTIONS ========================

source("master.R");

## ============== START ===========
print("======= Starting ========")

log_dir02 <- "~/gmtp/app/ns-3-dce/results/sim-2"
client_files02 <- paste(log_dir02, "/client-*.log", sep = "")
server_files02 <- paste(log_dir02, "/server-*.log", sep = "")

clients_logs02 <- Sys.glob(client_files02)
clients_len02 <- length(clients_logs02)
clients02 <- table_from_files(clients_logs02, "idx")

seq_gmtp02 <- sub_table(clients02, 2, "idx", 7)
loss_gmtp02 <- sub_table(clients02, 3, "idx", 7)
elapsed_gmtp02 <- sub_table(clients02, 4, "idx", 7)
rate_gmtp02 <- sub_table(clients02, 6, "idx", 7)
inst_rate_gmtp02 <- sub_table(clients02, 7, "idx", 7)
inst_rate_gmtp02 <- na.omit(inst_rate_gmtp02)
ndp_clients02 <- sub_table(clients02, 8, "idx", 7)

server_logs02 <- Sys.glob(server_files02)
server_len02 <- length(server_logs02)
server02 <- table_from_files(server_logs02, "idx")

ndp_server02 <- sub_table(server02, 2, "idx", 1)

## ============== LOSSES ===========
report(seq_gmtp02$mean)
plot(seq_gmtp02$mean, type="n", main="GMTP - Número de Sequencia", xlab="Pacotes Recebidos", ylab="Número de Sequencia")
lines(seq_gmtp02$mean, lwd=3)
lines(clients02$idx, col="red", lwd=2)

report(loss_gmtp02$mean)
n <- 0
tot_loss02 <- c()
for(i in 2:ncol(loss_gmtp02)-1) {
  loss <- sum(loss_gmtp02[i]) / seq_gmtp02[nrow(seq_gmtp02), i]
  tot_loss02[n] <- loss
  n <- n + 1
}
report(tot_loss02)
loss_rate02 <- mean(tot_loss02)*100
lg02 <- tot_loss02
#lg <- get_mean_table(loss_gmtp);
#report(lg)

report(elapsed_gmtp02$mean)
plot(elapsed_gmtp02$mean, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Pacotes Recebidos", ylab="Intervalo entre dois pacotes (ms)")
points(elapsed_gmtp02$mean)
lines(lowess(elapsed_gmtp02$mean), col="yellow", lwd=3)
abline(lm(elapsed_gmtp02$mean~clients02$idx), col="green", lwd=3)

## ============== RX RATE ===========
rate_gmtp02$mean[nrow(rate_gmtp02)]

rg02 <- last_line(rate_gmtp02);
report(rg02)

report(inst_rate_gmtp02$mean)
plot_graph(inst_rate_gmtp02$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg02 <- get_mean_table(inst_rate_gmtp02)
report(irg02)

ndpc02 <-last_line(ndp_clients02);
report(ndpc02)
ndps02 <-last_line(ndp_server02);
report(ndps02)

c_ndp02 <- ceiling(ndp_clients02$mean[nrow(ndp_clients02)] + 2 * sum(elapsed_gmtp02$mean)/1000)
s_ndp02 <- ceiling(ndp_server02$mean[nrow(ndp_server02)])
ndp02 <- c_ndp02 + s_ndp02

