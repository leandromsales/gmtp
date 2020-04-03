## ========================= FUNCTIONS ========================

source("~/gmtp/app/ns-3-dce/analysis/master.R");

## ============== START ===========
print("======= Starting ========")

log_dir05 <- "~/gmtp/app/ns-3-dce/results/sim-5"
client_files05 <- paste(log_dir05, "/client-*.log", sep = "")
server_files05 <- paste(log_dir05, "/server-*.log", sep = "")

clients_logs05 <- Sys.glob(client_files05)
clients_len05 <- length(clients_logs05)
clients05 <- table_from_files(clients_logs05, "idx")

seq_gmtp05 <- sub_table(clients05, 2, "idx", 7)
loss_gmtp05 <- sub_table(clients05, 3, "idx", 7)
elapsed_gmtp05 <- sub_table(clients05, 4, "idx", 7)
rate_gmtp05 <- sub_table(clients05, 6, "idx", 7)
inst_rate_gmtp05 <- sub_table(clients05, 7, "idx", 7)
inst_rate_gmtp05 <- na.omit(inst_rate_gmtp05)
ndp_clients05 <- sub_table(clients05, 8, "idx", 7)

server_logs05 <- Sys.glob(server_files05)
server_len05 <- length(server_logs05)
server05 <- table_from_files(server_logs05, "idx")

ndp_server05 <- sub_table(server05, 2, "idx", 1)
## ============== LOSSES ===========
report(seq_gmtp05$mean)
print_seq_graph(seq_gmtp05$mean, clients05$idx)

tot_loss05 <- losses(loss_gmtp05, seq_gmtp05)
report(tot_loss05)
loss_rate05 <- loss_rate(tot_loss05)
loss_ic05 <- poisson.interval(nrow(clients05)*tot_loss05, nrow(clients05))

## ============== CONTINUIDADE ===========
report(elapsed_gmtp05$mean)
print_elapsed(elapsed_gmtp05$mean, clients05$idx)
contin05 <- continuidade(elapsed_gmtp05, 7, "mean", loss_rate05)

## ============== RX RATE ===========
rate_gmtp05$mean[nrow(rate_gmtp05)]

rg05 <- last_line(rate_gmtp05);
report(rg05)

report(inst_rate_gmtp05$mean)
plot_graph(inst_rate_gmtp05$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg05 <- get_mean_table(inst_rate_gmtp05)
report(irg05)

##================ NDP =====================
#ndp05 <- ndp(ndp_clients05, ndp_server05, elapsed_gmtp05$mean)
#ndp_len05 <- ndp_len(ndp05)

# Error on logs... making projection
xserver <- c(1, 2)

yndp_clients5 <- c(ndp_clientn(ndp_clients02, ndp_server02, elapsed_gmtp02$mean), 
                   ndp_clientn(ndp_clients_aux02, ndp_server_aux02, elapsed_gmtp_aux02$mean))

xclients5 <- c(10, 20)
ndpc5 <- project(xclients5, yndp_clients5, 30)

yserver5 <- c(ndp_servern(ndp_server02), ndp_servern(ndp_server_aux02))
ndps5 <- project(xserver, yserver5, 3)

ndp05 <- ndp2(ndpc5[1], ndps5[1])
ndp_len05 <- ndp_len(ndp05)

