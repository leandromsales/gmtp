## ========================= FUNCTIONS ========================

source("~/gmtp/app/ns-3-dce/analysis/master.R");

## ============== START ===========
print("======= Starting ========")

log_dir_aux01 <- "~/gmtp/app/ns-3-dce/results/aux-1"
client_files_aux01 <- paste(log_dir_aux01, "/client-*.log", sep = "")
server_files_aux01 <- paste(log_dir_aux01, "/server-*.log", sep = "")

clients_logs_aux01 <- Sys.glob(client_files_aux01)
clients_len_aux01 <- length(clients_logs_aux01)
clients_aux01 <- table_from_files(clients_logs_aux01, "idx")

seq_gmtp_aux01 <- sub_table(clients_aux01, 2, "idx", 7)
loss_gmtp_aux01 <- sub_table(clients_aux01, 3, "idx", 7)
elapsed_gmtp_aux01 <- sub_table(clients_aux01, 4, "idx", 7)
rate_gmtp_aux01 <- sub_table(clients_aux01, 6, "idx", 7)
inst_rate_gmtp_aux01 <- sub_table(clients_aux01, 7, "idx", 7)
inst_rate_gmtp_aux01 <- na.omit(inst_rate_gmtp_aux01)
ndp_clients_aux01 <- sub_table(clients_aux01, 8, "idx", 7)

server_logs_aux01 <- Sys.glob(server_files_aux01)
server_len_aux01 <- length(server_logs_aux01)
server_aux01 <- table_from_files(server_logs_aux01, "idx")

ndp_server_aux01 <- sub_table(server_aux01, 2, "idx", 1)

## ============== LOSSES ===========
report(seq_gmtp_aux01$mean)
print_seq_graph(seq_gmtp_aux01$mean, clients_aux01$idx)

tot_loss_aux01 <- losses(loss_gmtp_aux01, seq_gmtp_aux01)
report(tot_loss_aux01)
loss_rate_aux01 <- loss_rate(tot_loss_aux01)

## ============== CONTINUIDADE ===========
report(elapsed_gmtp_aux01$mean)
print_elapsed(elapsed_gmtp_aux01$mean, clients_aux01$idx)
contin_aux01 <- continuidade(elapsed_gmtp_aux01, 7, "mean", loss_rate_aux01)

## ============== RX RATE ===========
rate_gmtp_aux01$mean[nrow(rate_gmtp_aux01)]

rg_aux01 <- last_line(rate_gmtp_aux01);
report(rg_aux01)

report(inst_rate_gmtp_aux01$mean)
plot_graph(inst_rate_gmtp_aux01$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg_aux01 <- get_mean_table(inst_rate_gmtp_aux01)
report(irg_aux01)

ndpc_aux01 <-last_line(ndp_clients_aux01);
report(ndpc_aux01)
ndps_aux01 <-last_line(ndp_server_aux01);
report(ndps_aux01)

c_ndp_aux01 <- ceiling(ndp_clients_aux01$mean[nrow(ndp_clients_aux01)] + 2 * sum(elapsed_gmtp_aux01$mean)/1000)
s_ndp_aux01 <- ceiling(ndp_server_aux01$mean[nrow(ndp_server_aux01)])
ndp_aux01 <- c_ndp_aux01 + s_ndp_aux01


