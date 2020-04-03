## ========================= FUNCTIONS ========================

source("~/gmtp/app/ns-3-dce/analysis/master.R");

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
print_seq_graph(seq_gmtp01$mean, clients01$idx)

tot_loss01 <- losses(loss_gmtp01, seq_gmtp01)
report(tot_loss01)
loss_rate01 <- loss_rate(tot_loss01)
loss_ic01 <- poisson.interval(nrow(clients01)*tot_loss01, nrow(clients01))

## ============== CONTINUIDADE ===========
report(elapsed_gmtp01$mean)
print_elapsed(elapsed_gmtp01$mean, clients01$idx)
contin01 <- continuidade(elapsed_gmtp01, 7, "mean", loss_rate01)

## ============== RX RATE ===========
rate_gmtp01$mean[nrow(rate_gmtp01)]

rg01 <- last_line(rate_gmtp01);
report(rg01)

report(inst_rate_gmtp01$mean)
plot_graph(inst_rate_gmtp01$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg01 <- get_mean_table(inst_rate_gmtp01)
report(irg01)

##================ NDP =====================
ndp01 <- ndp(ndp_clients01, ndp_server01, elapsed_gmtp01$mean)
ndp_len01 <- ndp_len(ndp01)


