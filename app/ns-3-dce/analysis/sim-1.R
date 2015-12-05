## ========================= FUNCTIONS ========================

source("master.R");

## ============== START ===========
print("======= Starting ========")

log_dir01 <- "~/gmtp/app/ns-3-dce/results/sim-1"
client_files01 <- paste(log_dir01, "/client-*.log", sep = "")
server_files01 <- paste(log_dir01, "/server-*.log", sep = "")

clients_logs01 <- Sys.glob(client_files01)
clients_len01 <- length(clients_logs01)
clients01 <- table_from_files(clients_logs01, "idx")

seq_gmtp01 <- sub_table(clients01, 2, "idx", 7)
loss_gmtp01 <- sub_table(clients01, 3, "idx", 7)
elapsed_gmtp01 <- sub_table(clients01, 4, "idx", 7)
rate_gmtp01 <- sub_table(clients01, 6, "idx", 7)
inst_rate_gmtp01 <- sub_table(clients01, 7, "idx", 7)
inst_rate_gmtp01 <- na.omit(inst_rate_gmtp01)
ndp_clients01 <- sub_table(clients01, 8, "idx", 7)

server_logs01 <- Sys.glob(server_files01)
server_len01 <- length(server_logs01)
server01 <- table_from_files(server_logs01, "idx")

ndp_server01 <- sub_table(server01, 2, "idx", 1)

## ============== LOSSES ===========
report(seq_gmtp01$mean)
plot(seq_gmtp01$mean, type="n", main="GMTP - Número de Sequencia", xlab="Pacotes Recebidos", ylab="Número de Sequencia")
lines(seq_gmtp01$mean, lwd=3)
lines(clients01$idx, col="red", lwd=2)

report(loss_gmtp01$mean)
n <- 0
tot_loss01 <- c()
for(i in 2:ncol(loss_gmtp01)-1) {
  loss <- sum(loss_gmtp01[i]) / seq_gmtp01[nrow(seq_gmtp01), i]
  tot_loss01[n] <- loss
  n <- n + 1
}
report(tot_loss01)
loss_rate01 <- mean(tot_loss01)*100
lg01 <- tot_loss01
#lg <- get_mean_table(loss_gmtp);
#report(lg)

report(elapsed_gmtp01$mean)
plot(elapsed_gmtp01$mean, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Pacotes Recebidos", ylab="Intervalo entre dois pacotes (ms)")
points(elapsed_gmtp01$mean)
lines(lowess(elapsed_gmtp01$mean), col="yellow", lwd=3)
abline(lm(elapsed_gmtp01$mean~clients01$idx), col="green", lwd=3)

## ============== RX RATE ===========
rate_gmtp01$mean[nrow(rate_gmtp01)]

rg01 <- last_line(rate_gmtp01);
report(rg01)

report(inst_rate_gmtp01$mean)
plot_graph(inst_rate_gmtp01$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg01 <- get_mean_table(inst_rate_gmtp01)
report(irg01)

ndpc01 <-last_line(ndp_clients01);
report(ndpc01)
ndps01 <-last_line(ndp_server01);
report(ndps01)

c_ndp01 <- ceiling(ndp_clients01$mean[nrow(ndp_clients01)] + 2 * sum(elapsed_gmtp01$mean)/1000)
s_ndp01 <- ceiling(ndp_server01$mean[nrow(ndp_server01)])
ndp01 <- c_ndp01 + s_ndp01


