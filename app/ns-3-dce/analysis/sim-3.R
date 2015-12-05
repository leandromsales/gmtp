## ========================= FUNCTIONS ========================

source("master.R");

## ============== START ===========
print("======= Starting ========")

log_dir03 <- "~/gmtp/app/ns-3-dce/results/sim-3"
client_files03 <- paste(log_dir03, "/client-*.log", sep = "")
server_files03 <- paste(log_dir03, "/server-*.log", sep = "")

clients_logs03 <- Sys.glob(client_files03)
clients_len03 <- length(clients_logs03)
clients03 <- table_from_files(clients_logs03, "idx")

seq_gmtp03 <- sub_table(clients03, 2, "idx", 7)
loss_gmtp03 <- sub_table(clients03, 3, "idx", 7)
elapsed_gmtp03 <- sub_table(clients03, 4, "idx", 7)
rate_gmtp03 <- sub_table(clients03, 6, "idx", 7)
inst_rate_gmtp03 <- sub_table(clients03, 7, "idx", 7)
inst_rate_gmtp03 <- na.omit(inst_rate_gmtp03)
ndp_clients03 <- sub_table(clients03, 8, "idx", 7)

server_logs03 <- Sys.glob(server_files03)
server_len03 <- length(server_logs03)
server03 <- table_from_files(server_logs03, "idx")

ndp_server03 <- sub_table(server03, 2, "idx", 1)

## ============== LOSSES ===========
report(seq_gmtp03$mean)
plot(seq_gmtp03$mean, type="n", main="GMTP - Número de Sequencia", xlab="Pacotes Recebidos", ylab="Número de Sequencia")
lines(seq_gmtp03$mean, lwd=3)
lines(clients03$idx, col="red", lwd=2)

report(loss_gmtp03$mean)
n <- 0
tot_loss03 <- c()
for(i in 2:ncol(loss_gmtp03)-1) {
  loss <- sum(loss_gmtp03[i]) / seq_gmtp03[nrow(seq_gmtp03), i]
  tot_loss03[n] <- loss
  n <- n + 1
}
report(tot_loss03)
loss_rate03 <- mean(tot_loss03)*100
lg03 <- tot_loss03
#lg <- get_mean_table(loss_gmtp);
#report(lg)

report(elapsed_gmtp03$mean)
plot(elapsed_gmtp03$mean, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Pacotes Recebidos", ylab="Intervalo entre dois pacotes (ms)")
points(elapsed_gmtp03$mean)
lines(lowess(elapsed_gmtp03$mean), col="yellow", lwd=3)
abline(lm(elapsed_gmtp03$mean~clients03$idx), col="green", lwd=3)

## ============== RX RATE ===========
rate_gmtp03$mean[nrow(rate_gmtp03)]

rg03 <- last_line(rate_gmtp03);
report(rg03)

report(inst_rate_gmtp03$mean)
plot_graph(inst_rate_gmtp03$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg03 <- get_mean_table(inst_rate_gmtp03)
report(irg03)

ndpc03 <-last_line(ndp_clients03);
report(ndpc03)
ndps03 <-last_line(ndp_server03);
report(ndps03)

c_ndp03 <- ceiling(ndp_clients03$mean[nrow(ndp_clients03)] + 2 * sum(elapsed_gmtp03$mean)/1000)
s_ndp03 <- ceiling(ndp_server03$mean[nrow(ndp_server03)])
ndp03 <- c_ndp03 + s_ndp03

