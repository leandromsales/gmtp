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

##================ NDP =====================
#ndp06 <- ndp(ndp_clients06, ndp_server06, elapsed_gmtp06$mean)
#ndp_len06 <- ndp_len(ndp06)

# Error on logs... making projection
xserver <- c(1, 2)

yndp_clients6 <- c(ndp_clientn(ndp_clients03, ndp_server03, elapsed_gmtp03$mean), 
                   ndp_clientn(ndp_clients_aux03, ndp_server_aux03, elapsed_gmtp_aux03$mean))

xclients6 <- c(20, 40)
ndpc6 <- project(xclients6, yndp_clients6, 60)

yserver6 <- c(ndp_servern(ndp_server03), ndp_servern(ndp_server_aux03))
ndps6 <- project(xserver, yserver6, 3)

ndp06 <- ndp2(ndpc6[1], ndps6[1])
ndp_len06 <- ndp_len(ndp06)


