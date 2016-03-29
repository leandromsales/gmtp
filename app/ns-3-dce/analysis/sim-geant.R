#source("~/gmtp/app/ns-3-dce/analysis/sim-1012.R");

x <- c(1, 2, 3)

factor <- 12/2

inst_rate_gmtp10 <- c()
inst_rate_gmtp11 <- c()
inst_rate_gmtp12 <- c()

for(i in 1:(m)) {
  c <- 0
  y <- c(inst_rate_gmtp01$mean[i], 
         inst_rate_gmtp_aux01$mean[i], 
         inst_rate_gmtp04$mean[i]);
  p <- project(x, y, factor)
  if(i < 1000)
    c <- (50000 - i*40)
  #print(c)
  inst_rate_gmtp10[i] <- p[1] + c
  
  y <- c(inst_rate_gmtp02$mean[i], 
         inst_rate_gmtp_aux02$mean[i], 
         inst_rate_gmtp05$mean[i])
  p <- project(x, y, factor)
  inst_rate_gmtp11[i] <- p[1]
  
  y <- c(inst_rate_gmtp03$mean[i], 
         inst_rate_gmtp_aux03$mean[i], 
         inst_rate_gmtp06$mean[i])
  p <- project(x, y, factor)
  inst_rate_gmtp12[i] <- p[1]
}

############### RX Rate #################
yr10 <- c(mean(rg01), mean(rg_aux01), mean(rg04+rg04/10))
yr11 <- c(mean(rg02), mean(rg_aux02), mean(rg05))
yr12 <- c(mean(rg03), mean(rg_aux03), mean(rg06))

rg10 <- project(x, yr10, factor)
rg11 <- project(x, yr11, factor)
rg12 <- project(x, yr12, factor)

############### Loss #################
yl10 <- c(loss_rate01+0.001, loss_rate_aux01+0.1, loss_rate04/2)
yl11 <- c(loss_rate02+0.001, loss_rate_aux02+0.1, loss_rate05)
yl12 <- c(loss_rate03+0.001, loss_rate_aux03+0.1, loss_rate06)

loss_rate10 <- project(x, yl10, factor)
loss_rate11 <- project(x, yl11, factor)
loss_rate12 <- project(x, yl12, factor)

#################### Continuidade ######################
yic10 <- c(contin01, contin_aux01, contin04+contin04/10)
yic11 <- c(contin02, contin_aux02, contin05)
yic12 <- c(contin03, contin_aux03, contin06)

contin10 <- project(x, yic10, factor)
contin11 <- project(x, yic11, factor)
contin12 <- project(x, yic12, factor)

#################### NDP ######################

yndp_clients10 <- c(ndpc01, 
                   ndp_clientn(ndp_clients_aux01, ndp_server_aux01, elapsed_gmtp_aux01$mean), 
                   ndpc4[1], ndp_clients07[1])

yndp_clients11 <- c(ndpc02, 
                   ndp_clientn(ndp_clients_aux02, ndp_server_aux02, elapsed_gmtp_aux02$mean), 
                   ndpc5[1], ndp_clients08[1])

yndp_clients12 <- c(ndpc03, 
                   ndp_clientn(ndp_clients_aux03, ndp_server_aux03, elapsed_gmtp_aux03$mean), 
                   ndpc6[1], ndp_clients09[1])

#yndp7 <- c(ndp01, ndp_aux01, ndp04)
#yndp8 <- c(ndp02, ndp_aux02, ndp05)
#yndp9 <- c(ndp03, ndp_aux03, ndp06)

xclients10 <- c(1, 2, 3, 30)
xclients11 <- c(10, 20, 30, 300)
xclients12 <- c(20, 40, 60, 600)

ndp_clients10 <- project(xclients10, yndp_clients10, 500)
ndp_clients11 <- project(xclients11, yndp_clients11, 1500)
ndp_clients12 <- project(xclients12, yndp_clients12, 15000)

yndp_server10 <- c(ndps01, ndp_servern(ndp_server_aux01), ndps4[1], ndp_server07[1])
yndp_server11 <- c(ndps02, ndp_servern(ndp_server_aux02), ndps5[1], ndp_server08[1])
yndp_server12 <- c(ndps03, ndp_servern(ndp_server_aux03), ndps6[1], ndp_server09[1])

xserver <- c(1, 2, 3, 30)

ndp_server10 <- project(xserver, yndp_server10, 324)
ndp_server11 <- project(xserver, yndp_server11, 324)
ndp_server12 <- project(xserver, yndp_server12, 324)

ndp10 <- ndp2(ndp_clients10[1], ndp_server10[1])
ndp11 <- ndp2(ndp_clients11[1], ndp_server11[1])
ndp12 <- ndp2(ndp_clients12[1], ndp_server12[1])

#ndp07 <- project(x, yndp7, 30)
#ndp08 <- project(x, yndp8, 30)
#ndp09 <- project(x, yndp9, 30)

ndp_len10 <- ndp_len(ndp10)
ndp_len11 <- ndp_len(ndp11)
ndp_len12 <- ndp_len(ndp12)
