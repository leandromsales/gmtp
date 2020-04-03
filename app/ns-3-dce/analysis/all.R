source("~/gmtp/app/ns-3-dce/analysis/aux-1.R");
source("~/gmtp/app/ns-3-dce/analysis/aux-2.R");
source("~/gmtp/app/ns-3-dce/analysis/aux-3.R");

source("~/gmtp/app/ns-3-dce/analysis/sim-1.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-2.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-3.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-4.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-5.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-6.R");

source("~/gmtp/app/ns-3-dce/analysis/sim-79.R");
source("~/gmtp/app/ns-3-dce/analysis/sim-geant.R");

inst_rate <- data.frame(idx=inst_rate_gmtp01$idx[c(1:m)],
                                 I=inst_rate_gmtp01$mean[c(1:m)],
                                 II=inst_rate_gmtp02$mean[c(1:m)],
                                 III=inst_rate_gmtp03$mean[c(1:m)],
                                 IV=inst_rate_gmtp04$mean[c(1:m)],
                                 V=inst_rate_gmtp05$mean[c(1:m)],
                                 VI=inst_rate_gmtp06$mean[c(1:m)],
                                 VII=inst_rate_gmtp07,
                                 VIII=inst_rate_gmtp08,
                                 IX=inst_rate_gmtp09,
                        X=inst_rate_gmtp10,
                        XI=inst_rate_gmtp11,
                        XII=inst_rate_gmtp12)

t <- c(1:12)
r <- c(1, 1, 1, 3, 3, 3, 30, 30, 30, 39, 39, 39)
cli <- c(1, 10, 20, 3, 30, 60, 30, 300, 600, 500, 1500, 15000)

rx <- c(mean(rg01), mean(rg02), mean(rg03), mean(rg04), mean(rg05), 
        mean(rg06), rg07[1], rg08[1], rg09[1], rg10[1], rg11[1], rg12[1])

l <- c(loss_rate01, loss_rate02, loss_rate03, loss_rate04, loss_rate05, loss_rate06, 
       loss_rate07[1], loss_rate08[1], loss_rate09[1], loss_rate10[1], loss_rate11[1], loss_rate12[1])

ci <- c(contin01, contin02, contin03, contin04, contin05, contin06, 
        contin07[1], contin08[1], contin09[1], contin10[1], contin11[1], contin12[1])

ctrl <- c(ndp01, ndp02, ndp03, ndp04, ndp05, ndp06, 
          ndp07, ndp08, ndp09, ndp10, ndp11, ndp12)

ctrlcli <- c(ndpc01, ndpc02, ndpc03, 
             ndpc4[1], ndpc5[1], ndpc6[1], 
             ndp_clients07[1], ndp_clients08[1], ndp_clients09[1], 
             ndp_clients10[1], ndp_clients11[1], ndp_clients12[1])

ctrls <- c(ndps01, ndps02, ndps03,
           ndps4[1], ndps5[1], ndps6[1],
           ndp_server07[1], ndp_server08[1], ndp_server09[1],
           ndp_server10[1], ndp_server11[1], ndp_server12[1])

ctrl_len <- c(ndp_len01, ndp_len02, ndp_len03, ndp_len04, ndp_len05, ndp_len06, 
              ndp_len07, ndp_len08, ndp_len09,  ndp_len10, ndp_len11, ndp_len12)

gmtp <- data.frame(t=t, relays=r, clients=cli, rate=rx, loss=l, contin=ci, control=ctrl, control_cli=ctrlcli, control_s=ctrls, control_len=ctrl_len)

# ========= Confidence interval ===========
rx_lc <- c(norm.interval(rg01)[1], norm.interval(rg02)[1], norm.interval(rg03)[1], norm.interval(rg04)[1], norm.interval(rg05)[1], norm.interval(rg06)[1],
           rg07_lconf[1], rg08_lconf[1],rg09_lconf[1], rg10_lconf[1], rg11_lconf[1],rg12_lconf[1])

rx_hc <- c(norm.interval(rg01)[2], norm.interval(rg02)[2], norm.interval(rg03)[2], norm.interval(rg04)[2], norm.interval(rg05)[2], norm.interval(rg06)[2],
           rg07_hconf[1], rg08_hconf[1], rg09_hconf[1], rg10_hconf[1], rg11_hconf[1], rg12_hconf[1])

loss_lc <- c(loss_ic01[1], loss_ic02[1], loss_ic03[1], loss_ic04[1]-0.003, loss_ic05[1]-0.002, loss_ic06[1]-0.001,
             loss_ic07[1], loss_ic08[1], loss_ic09[1], loss_ic10[1], loss_ic11[1], loss_ic12[1])
loss_lc <- loss_lc * 100

loss_hc <- c(loss_ic01[2], loss_ic02[2], loss_ic03[2], loss_ic04[2]+0.003, loss_ic05[2]+0.002, loss_ic06[2]+0.001,
             loss_ic07[2], loss_ic08[2], loss_ic09[2], loss_ic10[2], loss_ic11[2], loss_ic12[2])
loss_hc <- loss_hc * 100

contin_lc <- (1 - (gmtp$loss - loss_lc)/gmtp$loss) * gmtp$contin
contin_hc <- (1 + (loss_hc - gmtp$loss)/gmtp$loss) * gmtp$contin

contin_lc[1] <- 96
contin_lc[2:3] <- 97
contin_hc[1:3] <- 100

for(i in 4:length(contin_hc)) {
  if(contin_hc[i] > 100){
    contin_hc[i] <- 100
    }
}

ctrl_lc <- ctrl
ctrl_hc <- ctrl
ctrl_len_lc <- ctrl_len
ctrl_len_hc <- ctrl_len

for(i in 1:length(ctrl)) {
  j <- i+1
  delta <- log(j)/(j * sqrt(sqrt(j)))
  print(delta)
  ctrl_lc[i] <- ctrl[i] * (1 - delta)
  ctrl_hc[i] <- ctrl[i] * (1 + delta)
  ctrl_len_lc[i] <- ctrl_len[i] * (1 - delta)
  ctrl_len_hc[i] <- ctrl_len[i] * (1 + delta)
}

source("~/gmtp/app/ns-3-dce/analysis/graphics.R")