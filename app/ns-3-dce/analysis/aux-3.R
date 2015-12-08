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
print_seq_graph(seq_gmtp_aux03$mean, clients_aux03$idx)

tot_loss_aux03 <- losses(loss_gmtp_aux03, seq_gmtp_aux03)
report(tot_loss_aux03)
loss_rate_aux03 <- loss_rate(tot_loss_aux03)

## ============== CONTINUIDADE ===========
report(elapsed_gmtp_aux03$mean)
print_elapsed(elapsed_gmtp_aux03$mean, clients_aux03$idx)
contin_aux03 <- continuidade(elapsed_gmtp_aux03, 7, "mean", loss_rate_aux03)

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


