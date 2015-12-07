## ========================= FUNCTIONS ========================

source("~/gmtp/app/ns-3-dce/analysis/master.R");

## ============== START ===========
print("======= Starting ========")

log_dir_aux02 <- "~/gmtp/app/ns-3-dce/results/aux-2"
client_files_aux02 <- paste(log_dir_aux02, "/client-*.log", sep = "")
server_files_aux02 <- paste(log_dir_aux02, "/server-*.log", sep = "")

clients_logs_aux02 <- Sys.glob(client_files_aux02)
clients_len_aux02 <- length(clients_logs_aux02)
clients_aux02 <- table_from_files(clients_logs_aux02, "idx")

seq_gmtp_aux02 <- sub_table(clients_aux02, 2, "idx", 7)
loss_gmtp_aux02 <- sub_table(clients_aux02, 3, "idx", 7)
elapsed_gmtp_aux02 <- sub_table(clients_aux02, 4, "idx", 7)
rate_gmtp_aux02 <- sub_table(clients_aux02, 6, "idx", 7)
inst_rate_gmtp_aux02 <- sub_table(clients_aux02, 7, "idx", 7)
inst_rate_gmtp_aux02 <- na.omit(inst_rate_gmtp_aux02)
ndp_clients_aux02 <- sub_table(clients_aux02, 8, "idx", 7)

server_logs_aux02 <- Sys.glob(server_files_aux02)
server_len_aux02 <- length(server_logs_aux02)
server_aux02 <- table_from_files(server_logs_aux02, "idx")

ndp_server_aux02 <- sub_table(server_aux02, 2, "idx", 1)

## ============== LOSSES ===========
report(seq_gmtp_aux02$mean)
plot(seq_gmtp_aux02$mean, type="n", main="GMTP - Número de Sequencia", xlab="Pacotes Recebidos", ylab="Número de Sequencia")
lines(seq_gmtp_aux02$mean)
lines(clients_aux02$idx, col="red")

report(loss_gmtp_aux02$mean)
n <- 0
tot_loss_aux02 <- c()
for(i in 2:ncol(loss_gmtp_aux02)-1) {
  loss <- sum(loss_gmtp_aux02[i]) / seq_gmtp_aux02[nrow(seq_gmtp_aux02), i]
  tot_loss_aux02[n] <- loss
  n <- n + 1
}
report(tot_loss_aux02)
loss_rate_aux02 <- mean(tot_loss_aux02)*100
lg_aux02 <- tot_loss_aux02
#lg <- get_mean_table(loss_gmtp);
#report(lg)

report(elapsed_gmtp_aux02$mean)
plot(elapsed_gmtp_aux02$mean, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Pacotes Recebidos", ylab="Intervalo entre dois pacotes (ms)")
points(elapsed_gmtp_aux02$mean)
lines(lowess(elapsed_gmtp_aux02$mean), col="yellow")
abline(lm(elapsed_gmtp_aux02$mean~clients_aux02$idx), col="green")

## ============== RX RATE ===========
rate_gmtp_aux02$mean[nrow(rate_gmtp_aux02)]

rg_aux02 <- last_line(rate_gmtp_aux02);
report(rg_aux02)

report(inst_rate_gmtp_aux02$mean)
plot_graph(inst_rate_gmtp_aux02$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg_aux02 <- get_mean_table(inst_rate_gmtp_aux02)
report(irg_aux02)

ndpc_aux02 <-last_line(ndp_clients_aux02);
report(ndpc_aux02)
ndps_aux02 <-last_line(ndp_server_aux02);
report(ndps_aux02)

c_ndp_aux02 <- ceiling(ndp_clients_aux02$mean[nrow(ndp_clients_aux02)] + 2 * sum(elapsed_gmtp_aux02$mean)/1000)
s_ndp_aux02 <- ceiling(ndp_server_aux02$mean[nrow(ndp_server_aux02)])
ndp_aux02 <- c_ndp_aux02 + s_ndp_aux02


