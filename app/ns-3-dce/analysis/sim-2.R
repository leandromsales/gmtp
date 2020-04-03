## ========================= FUNCTIONS ========================

source("~/gmtp/app/ns-3-dce/analysis/master.R");

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
print_seq_graph(seq_gmtp02$mean, clients02$idx)

tot_loss02 <- losses(loss_gmtp02, seq_gmtp02)
report(tot_loss02)
loss_rate02 <- loss_rate(tot_loss02)
loss_ic02 <- poisson.interval(nrow(clients02)*tot_loss02, nrow(clients02))

## ============== CONTINUIDADE ===========
report(elapsed_gmtp02$mean)
print_elapsed(elapsed_gmtp02$mean, clients02$idx)
contin02 <- continuidade(elapsed_gmtp02, 7, "mean", loss_rate02)

## ============== RX RATE ===========
rate_gmtp02$mean[nrow(rate_gmtp02)]

rg02 <- last_line(rate_gmtp02);
report(rg02)

report(inst_rate_gmtp02$mean)
plot_graph(inst_rate_gmtp02$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg02 <- get_mean_table(inst_rate_gmtp02)
report(irg02)

##================ NDP =====================
ndp02 <- ndp(ndp_clients02, ndp_server02, elapsed_gmtp02$mean)
ndp_len02 <- ndp_len(ndp02)

