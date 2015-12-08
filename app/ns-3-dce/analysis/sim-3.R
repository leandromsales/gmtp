## ========================= FUNCTIONS ========================

source("~/gmtp/app/ns-3-dce/analysis/master.R");

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
print_seq_graph(seq_gmtp03$mean, clients03$idx)

tot_loss03 <- losses(loss_gmtp03, seq_gmtp03)
report(tot_loss03)
loss_rate03 <- loss_rate(tot_loss03)

## ============== CONTINUIDADE ===========
report(elapsed_gmtp03$mean)
print_elapsed(elapsed_gmtp03$mean, clients03$idx)
contin03 <- continuidade(elapsed_gmtp03, 7, "mean", loss_rate03)

## ============== RX RATE ===========
rate_gmtp03$mean[nrow(rate_gmtp03)]

rg03 <- last_line(rate_gmtp03);
report(rg03)

report(inst_rate_gmtp03$mean)
plot_graph(inst_rate_gmtp03$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg03 <- get_mean_table(inst_rate_gmtp03)
report(irg03)

##================ NDP =====================
ndp03 <- ndp(ndp_clients03, ndp_server03, elapsed_gmtp03$mean)
ndp_len03 <- ndp_len(ndp03)

