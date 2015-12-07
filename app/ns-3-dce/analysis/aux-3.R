## ========================= FUNCTIONS ========================

source("~/gmtp/app/ns-3-dce/analysis/master.R");

## ============== START ===========
print("======= Starting ========")

log_dir_aux03 <- "~/gmtp/app/ns-3-dce/results/aux-3"
client_files_aux03 <- paste(log_dir_aux03, "/client-*.log", sep = "")
server_files_aux03 <- paste(log_dir_aux03, "/server-*.log", sep = "")

clients_logs_aux03 <- Sys.glob(client_files_aux03)
clients_len_aux03 <- length(clients_logs_aux03)
clients_aux03 <- table_from_files(clients_logs_aux03, "idx")

seq_gmtp_aux03 <- sub_table(clients_aux03, 2, "idx", 7)
loss_gmtp_aux03 <- sub_table(clients_aux03, 3, "idx", 7)
elapsed_gmtp_aux03 <- sub_table(clients_aux03, 4, "idx", 7)
rate_gmtp_aux03 <- sub_table(clients_aux03, 6, "idx", 7)
inst_rate_gmtp_aux03 <- sub_table(clients_aux03, 7, "idx", 7)
inst_rate_gmtp_aux03 <- na.omit(inst_rate_gmtp_aux03)
ndp_clients_aux03 <- sub_table(clients_aux03, 8, "idx", 7)

server_logs_aux03 <- Sys.glob(server_files_aux03)
server_len_aux03 <- length(server_logs_aux03)
server_aux03 <- table_from_files(server_logs_aux03, "idx")

ndp_server_aux03 <- sub_table(server_aux03, 2, "idx", 1)

## ============== LOSSES ===========
report(seq_gmtp_aux03$mean)
plot(seq_gmtp_aux03$mean, type="n", main="GMTP - Número de Sequencia", xlab="Pacotes Recebidos", ylab="Número de Sequencia")
lines(seq_gmtp_aux03$mean)
lines(clients_aux03$idx, col="red")

report(loss_gmtp_aux03$mean)
n <- 0
tot_loss_aux03 <- c()
for(i in 2:ncol(loss_gmtp_aux03)-1) {
  loss <- sum(loss_gmtp_aux03[i]) / seq_gmtp_aux03[nrow(seq_gmtp_aux03), i]
  tot_loss_aux03[n] <- loss
  n <- n + 1
}
report(tot_loss_aux03)
loss_rate_aux03 <- mean(tot_loss_aux03)*100
lg_aux03 <- tot_loss_aux03
#lg <- get_mean_table(loss_gmtp);
#report(lg)

report(elapsed_gmtp_aux03$mean)
plot(elapsed_gmtp_aux03$mean, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Pacotes Recebidos", ylab="Intervalo entre dois pacotes (ms)")
points(elapsed_gmtp_aux03$mean)
lines(lowess(elapsed_gmtp_aux03$mean), col="yellow")
abline(lm(elapsed_gmtp_aux03$mean~clients_aux03$idx), col="green")

## ============== RX RATE ===========
rate_gmtp_aux03$mean[nrow(rate_gmtp_aux03)]

rg_aux03 <- last_line(rate_gmtp_aux03);
report(rg_aux03)

report(inst_rate_gmtp_aux03$mean)
plot_graph(inst_rate_gmtp_aux03$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg_aux03 <- get_mean_table(inst_rate_gmtp_aux03)
report(irg_aux03)

ndpc_aux03 <-last_line(ndp_clients_aux03);
report(ndpc_aux03)
ndps_aux03 <-last_line(ndp_server_aux03);
report(ndps_aux03)

c_ndp_aux03 <- ceiling(ndp_clients_aux03$mean[nrow(ndp_clients_aux03)] + 2 * sum(elapsed_gmtp_aux03$mean)/1000)
s_ndp_aux03 <- ceiling(ndp_server_aux03$mean[nrow(ndp_server_aux03)])
ndp_aux03 <- c_ndp_aux03 + s_ndp_aux03


