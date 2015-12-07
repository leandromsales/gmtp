source("~/gmtp/app/ns-3-dce/analysis/sim-1.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-2.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-3.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-4.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-5.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-6.R");

source("~/gmtp/app/ns-3-dce/analysis/sim-79.R");

inst_rate <- data.frame(idx=inst_rate_gmtp01$idx[c(1:m)],
                                 I=inst_rate_gmtp01$mean[c(1:m)],
                                 II=inst_rate_gmtp02$mean[c(1:m)],
                                 III=inst_rate_gmtp03$mean[c(1:m)],
                                 IV=inst_rate_gmtp04$mean[c(1:m)],
                                 V=inst_rate_gmtp05$mean[c(1:m)],
                                 VI=inst_rate_gmtp06$mean[c(1:m)],
                                 VII=inst_rate_gmtp07,
                                 VIII=inst_rate_gmtp08,
                                 IX=inst_rate_gmtp09)

t <- c(1:9)
r <- c(1, 1, 1, 3, 3, 3, 30, 30, 30)
cli <- c(1, 10, 20, 3, 30, 60, 30, 300, 600)
rx <- c(mean(rg01), mean(rg02), mean(rg03), mean(rg04), mean(rg05), mean(rg06), rg07[1], rg08[1], rg09[1])
l <- c(loss_rate01, loss_rate02, loss_rate03, loss_rate04, loss_rate05, loss_rate06, loss_rate07[1], loss_rate08[1], loss_rate09[1])
ci <- c(0, 0, 0, 0, 0, 0, 0, 0, 0)
ctrl <- c(0, 0, 0, 0, 0, 0, 0, 0, 0)
gmtp <- data.frame(t=t, relays=r, clients=cli, rate=rx, loss=l, contin=ci, control=ctrl)
