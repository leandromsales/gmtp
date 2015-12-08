## ========================= FUNCTIONS ========================

source("~/gmtp/app/ns-3-dce/analysis/master.R");

## ============== START ===========
print("======= Starting ========")

log_dir06 <- "~/gmtp/app/ns-3-dce/results/sim-6"
client_files06 <- paste(log_dir06, "/client-*.log", sep = "")
server_files06 <- paste(log_dir06, "/server-*.log", sep = "")

clients_logs06 <- Sys.glob(client_files06)
clients_len06 <- length(clients_logs06)
clients06 <- table_from_files(clients_logs06, "idx")

seq_gmtp06 <- sub_table(clients06, 2, "idx", 7)
loss_gmtp06 <- sub_table(clients06, 3, "idx", 7)
elapsed_gmtp06 <- sub_table(clients06, 4, "idx", 7)
rate_gmtp06 <- sub_table(clients06, 6, "idx", 7)
inst_rate_gmtp06 <- sub_table(clients06, 7, "idx", 7)
inst_rate_gmtp06 <- na.omit(inst_rate_gmtp06)
ndp_clients06 <- sub_table(clients06, 8, "idx", 7)

server_logs06 <- Sys.glob(server_files06)
server_len06 <- length(server_logs06)
server06 <- table_from_files(server_logs06, "idx")

ndp_server06 <- sub_table(server06, 2, "idx", 1)

## ============== LOSSES ===========
report(seq_gmtp06$mean)
print_seq_graph(seq_gmtp06$mean, clients06$idx)

tot_loss06 <- losses(loss_gmtp06, seq_gmtp06)
report(tot_loss06)
loss_rate06 <- loss_rate(tot_loss06)

## ============== CONTINUIDADE ===========
report(elapsed_gmtp06$mean)
print_elapsed(elapsed_gmtp06$mean, clients06$idx)
contin06 <- continuidade(elapsed_gmtp06, 7, "mean", loss_rate06)

## ============== RX RATE ===========
rate_gmtp06$mean[nrow(rate_gmtp06)]

rg06 <- last_line(rate_gmtp06);
report(rg06)

report(inst_rate_gmtp06$mean)
plot_graph(inst_rate_gmtp06$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg06 <- get_mean_table(inst_rate_gmtp06)
report(irg06)

ndpc06 <-last_line(ndp_clients06);
report(ndpc06)
ndps06 <-last_line(ndp_server06);
report(ndps06)

c_ndp06 <- ceiling(ndp_clients06$mean[nrow(ndp_clients06)] + 2 * sum(elapsed_gmtp06$mean)/1000)
s_ndp06 <- ceiling(ndp_server06$mean[nrow(ndp_server06)])
ndp06 <- c_ndp06 + s_ndp06

