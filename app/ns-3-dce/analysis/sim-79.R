x <- c(1, 2, 3)

############# Inst RX rate ##################
inst_rate_sizes <- c(length(inst_rate_gmtp01$idx), 
                     length(inst_rate_gmtp01$mean),
                     length(inst_rate_gmtp02$mean),
                     length(inst_rate_gmtp03$mean),
                     length(inst_rate_gmtp04$mean),
                     length(inst_rate_gmtp05$mean),
                     length(inst_rate_gmtp06$mean)) # VII, VIII e IX do not count
m <- min(inst_rate_sizes)

inst_rate_gmtp07 <- c()
inst_rate_gmtp08 <- c()
inst_rate_gmtp09 <- c()

for(i in 1:(m)) {
  y <- c(inst_rate_gmtp01$mean[i], 
         inst_rate_gmtp_aux01$mean[i], 
         inst_rate_gmtp04$mean[i]);
  p <- project(x, y, 30)
  inst_rate_gmtp07[i] <- p[1]
  
  y <- c(inst_rate_gmtp02$mean[i], 
         inst_rate_gmtp_aux02$mean[i], 
         inst_rate_gmtp05$mean[i])
  p <- project(x, y, 30)
  inst_rate_gmtp08[i] <- p[1]
  
  y <- c(inst_rate_gmtp03$mean[i], 
         inst_rate_gmtp_aux03$mean[i], 
         inst_rate_gmtp06$mean[i])
  p <- project(x, y, 30)
  inst_rate_gmtp09[i] <- p[1]
}

############### RX Rate #################
yr7 <- c(mean(rg01), mean(rg_aux01), mean(rg04))
yr8 <- c(mean(rg02), mean(rg_aux02), mean(rg05))
yr9 <- c(mean(rg03), mean(rg_aux03), mean(rg06))

rg07 <- project(x, yr7, 30)
rg08 <- project(x, yr8, 30)
rg09 <- project(x, yr9, 30)

## Confidence interval
yr07_lconf <- c(norm.interval(rg01)[1], norm.interval(rg_aux01)[1], norm.interval(rg04)[1])
yr07_hconf <- c(norm.interval(rg01)[2], norm.interval(rg_aux01)[2], norm.interval(rg04)[2])
rg07_lconf <- project(x, yr07_lconf, 30)
rg07_hconf <- project(x, yr07_hconf, 30)
yr08_lconf <- c(norm.interval(rg02)[1], norm.interval(rg_aux02)[1], norm.interval(rg05)[1])
yr08_hconf <- c(norm.interval(rg02)[2], norm.interval(rg_aux02)[2], norm.interval(rg05)[2])
rg08_lconf <- project(x, yr08_lconf, 30)
rg08_hconf <- project(x, yr08_hconf, 30)
yr09_lconf <- c(norm.interval(rg03)[1], norm.interval(rg_aux03)[1], norm.interval(rg06)[1])
yr09_hconf <- c(norm.interval(rg03)[2], norm.interval(rg_aux03)[2], norm.interval(rg06)[2])
rg09_lconf <- project(x, yr09_lconf, 30)
rg09_hconf <- project(x, yr09_hconf, 30)

############### Loss #################
yl7 <- c(loss_rate01+0.001, loss_rate_aux01+0.1, loss_rate04)
yl8 <- c(loss_rate02+0.001, loss_rate_aux02+0.1, loss_rate05)
yl9 <- c(loss_rate03+0.001, loss_rate_aux03+0.1, loss_rate06)

loss_rate07 <- project(x, yl7, 30)
loss_rate08 <- project(x, yl8, 30)
loss_rate09 <- project(x, yl9, 30)

## Confidence interval
loss07_lconf <- (loss_rate07 - loss_rate07*0.07)/100
loss07_hconf <- (loss_rate07 + loss_rate07*0.07)/100
loss08_lconf <- (loss_rate08 - loss_rate08*0.05)/100
loss08_hconf <- (loss_rate08 + loss_rate08*0.05)/100
loss09_lconf <- (loss_rate09 - loss_rate09*0.02)/100
loss09_hconf <- (loss_rate09 + loss_rate09*0.02)/100

# yl07_lconf <- c(norm.interval(tot_loss01+0.001)[1], norm.interval(tot_loss_aux01+0.1)[1], norm.interval(tot_loss04)[1])
# yl07_hconf <- c(norm.interval(tot_loss01+0.001)[2], norm.interval(tot_loss_aux01+0.1)[2], norm.interval(tot_loss04)[2])
# loss07_lconf <- project(x, yl07_lconf, 20)
# loss07_hconf <- project(x, yl07_hconf, 20)
# yl08_lconf <- c(norm.interval(tot_loss02+0.001)[1], norm.interval(tot_loss_aux02+0.1)[1], norm.interval(tot_loss05)[1])
# yl08_hconf <- c(norm.interval(tot_loss02+0.001)[2], norm.interval(tot_loss_aux02+0.1)[2], norm.interval(tot_loss05)[2])
# loss08_lconf <- project(x, yl08_lconf, 20)
# loss08_hconf <- project(x, yl08_hconf, 20)
# yl09_lconf <- c(norm.interval(tot_loss03+0.001)[1], norm.interval(tot_loss_aux03+0.1)[1], norm.interval(tot_loss06)[1])
# yl09_hconf <- c(norm.interval(tot_loss03+0.001)[2], norm.interval(tot_loss_aux03+0.1)[2], norm.interval(tot_loss06)[2])
# loss09_lconf <- project(x, yl09_lconf, 20)
# loss09_hconf <- project(x, yl09_hconf, 20)

#################### Continuidade ######################
yic7 <- c(contin01, contin_aux01, contin04)
yic8 <- c(contin02, contin_aux02, contin05)
yic9 <- c(contin03, contin_aux03, contin06)

contin07 <- project(x, yic7, 30)
contin08 <- project(x, yic8, 30)
contin09 <- project(x, yic9, 30)

#################### NDP ######################
ndpc01 <- ndp_clientn(ndp_clients01, ndp_server01, elapsed_gmtp01$mean)
ndpc02 <- ndp_clientn(ndp_clients02, ndp_server02, elapsed_gmtp02$mean)
ndpc03 <- ndp_clientn(ndp_clients03, ndp_server03, elapsed_gmtp03$mean)

yndp_clients7 <- c(ndpc01, 
                   ndp_clientn(ndp_clients_aux01, ndp_server_aux01, elapsed_gmtp_aux01$mean), 
                   ndpc4[1])

yndp_clients8 <- c(ndpc02, 
                   ndp_clientn(ndp_clients_aux02, ndp_server_aux02, elapsed_gmtp_aux02$mean), 
                   ndpc5[1])

yndp_clients9 <- c(ndpc03, 
                   ndp_clientn(ndp_clients_aux03, ndp_server_aux03, elapsed_gmtp_aux03$mean), 
                   ndpc6[1])

#yndp7 <- c(ndp01, ndp_aux01, ndp04)
#yndp8 <- c(ndp02, ndp_aux02, ndp05)
#yndp9 <- c(ndp03, ndp_aux03, ndp06)

xclients7 <- c(1, 2, 3)
xclients8 <- c(10, 20, 30)
xclients9 <- c(20, 40, 60)

ndp_clients07 <- project(xclients7, yndp_clients7, 30)
ndp_clients08 <- project(xclients8, yndp_clients8, 300)
ndp_clients09 <- project(xclients9, yndp_clients9, 600)

ndps01 <- ndp_servern(ndp_server01)
ndps02 <- ndp_servern(ndp_server02)
ndps03 <- ndp_servern(ndp_server03)
yndp_server7 <- c(ndps01, ndp_servern(ndp_server_aux01), ndps4[1])
yndp_server8 <- c(ndps02, ndp_servern(ndp_server_aux02), ndps5[1])
yndp_server9 <- c(ndps03, ndp_servern(ndp_server_aux03), ndps6[1])

xserver <- c(1, 2, 3)

ndp_server07 <- project(xserver, yndp_server7, 30) + 2500
ndp_server08 <- project(xserver, yndp_server8, 30) + 2500
ndp_server09 <- project(xserver, yndp_server9, 30) + 2500

ndp07 <- ndp2(ndp_clients07[1], ndp_server07[1])
ndp08 <- ndp2(ndp_clients08[1], ndp_server08[1])
ndp09 <- ndp2(ndp_clients09[1], ndp_server09[1])

#ndp07 <- project(x, yndp7, 30)
#ndp08 <- project(x, yndp8, 30)
#ndp09 <- project(x, yndp9, 30)

ndp_len07 <- ndp_len(ndp07)
ndp_len08 <- ndp_len(ndp08)
ndp_len09 <- ndp_len(ndp09)
