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
print_seq_graph(seq_gmtp04$mean, clients04$idx)

tot_loss04 <- losses(loss_gmtp04, seq_gmtp04)
report(tot_loss04)
loss_rate04 <- loss_rate(tot_loss04)
loss_ic04 <- poisson.interval(nrow(clients04)*tot_loss04, nrow(clients04))

## ============== CONTINUIDADE ===========
report(elapsed_gmtp04$mean)
print_elapsed(elapsed_gmtp04$mean, clients04$idx)
contin04 <- continuidade(elapsed_gmtp04, 7, "mean", loss_rate04)

## ============== RX RATE ===========
rate_gmtp04$mean[nrow(rate_gmtp04)]

rg04 <- last_line(rate_gmtp04);
report(rg04)

report(inst_rate_gmtp04$mean)
plot_graph(inst_rate_gmtp04$mean, "GMTP - Taxa de Recepção", "Taxa de Recepção (B/s)")
irg04 <- get_mean_table(inst_rate_gmtp04)
report(irg04)

##================ NDP =====================
#ndp04 <- ndp(ndp_clients04, ndp_server04, elapsed_gmtp04$mean)
#ndp_len04 <- ndp_len(ndp04)

# Error on logs... making projection
xserver <- c(1, 2)

yndp_clients4 <- c(ndp_clientn(ndp_clients01, ndp_server01, elapsed_gmtp01$mean), 
                   ndp_clientn(ndp_clients_aux01, ndp_server_aux01, elapsed_gmtp_aux01$mean))

xclients4 <- c(1, 2)
ndpc4 <- project(xclients4, yndp_clients4, 3)

yserver4 <- c(ndp_servern(ndp_server01), ndp_servern(ndp_server_aux01))
ndps4 <- project(xserver, yserver4, 3)

ndp04 <- ndp2(ndpc4[1], ndps4[1])
ndp_len04 <- ndp_len(ndp04)

