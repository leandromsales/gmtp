## ========================= FUNCTIONS ========================

source("~/gmtp/app/ns-3-dce/analysis/master.R");

## ============== START ===========
print("======= Starting ========")

log_dir04 <- "~/gmtp/app/ns-3-dce/results/sim-4"
client_files04 <- paste(log_dir04, "/client-*.log", sep = "")
server_files04 <- paste(log_dir04, "/server-*.log", sep = "")

clients_logs04 <- Sys.glob(client_files04)
clients_len04 <- length(clients_logs04)
clients04 <- table_from_files(clients_logs04, "idx")

seq_gmtp04 <- sub_table(clients04, 2, "idx", 7)
loss_gmtp04 <- sub_table(clients04, 3, "idx", 7)
elapsed_gmtp04 <- sub_table(clients04, 4, "idx", 7)
rate_gmtp04 <- sub_table(clients04, 6, "idx", 7)
inst_rate_gmtp04 <- sub_table(clients04, 7, "idx", 7)
inst_rate_gmtp04 <- na.omit(inst_rate_gmtp04)
ndp_clients04 <- sub_table(clients04, 8, "idx", 7)

server_logs04 <- Sys.glob(server_files04)
server_len04 <- length(server_logs04)
server04 <- table_from_files(server_logs04, "idx")

ndp_server04 <- sub_table(server04, 2, "idx", 1)

## ============== LOSSES ===========
report(seq_gmtp04$mean)
plot(seq_gmtp04$mean, type="n", main="GMTP - Número de Sequencia", xlab="Pacotes Recebidos", ylab="Número de Sequencia")
lines(seq_gmtp04$mean)
lines(clients04$idx, col="red")

report(loss_gmtp04$mean)
n <- 0
tot_loss04 <- c()
for(i in 2:ncol(loss_gmtp04)-1) {
  loss <- sum(loss_gmtp04[i]) / seq_gmtp04[nrow(seq_gmtp04), i]
  tot_loss04[n] <- loss
  n <- n + 1
}
report(tot_loss04)
loss_rate04 <- mean(tot_loss04)*100
lg04 <- tot_loss04
#lg <- get_mean_table(loss_gmtp);
#report(lg)

report(elapsed_gmtp04$mean)
plot(elapsed_gmtp04$mean, type="n", main="GMTP - Intervalo entre dois pacotes", xlab="Pacotes Recebidos", ylab="Intervalo entre dois pacotes (ms)")
points(elapsed_gmtp04$mean)
lines(lowess(elapsed_gmtp04$mean), col="yellow")
abline(lm(elapsed_gmtp04$mean~clients04$idx), col="green")

## ============== RX RATE ===========
rate_gmtp04$mean[nrow(rate_gmtp04)]

rg04 <- last_line(rate_gmtp04);
report(rg04)

report(inst_rate_gmtp04$mean)
plot_graph(inst_rate_gmtp04$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg04 <- get_mean_table(inst_rate_gmtp04)
report(irg04)

ndpc04 <-last_line(ndp_clients04);
report(ndpc04)
ndps04 <-last_line(ndp_server04);
report(ndps04)

c_ndp04 <- ceiling(ndp_clients04$mean[nrow(ndp_clients04)] + 2 * sum(elapsed_gmtp04$mean)/1000)
s_ndp04 <- ceiling(ndp_server04$mean[nrow(ndp_server04)])
ndp04 <- c_ndp04 + s_ndp04

