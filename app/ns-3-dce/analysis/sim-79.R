source("~/gmtp/app/ns-3-dce/analysis/aux-1.R");
source("~/gmtp/app/ns-3-dce/analysis/aux-2.R");
source("~/gmtp/app/ns-3-dce/analysis/aux-3.R");

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

############### Loss #################
yl7 <- c(loss_rate01+0.001, loss_rate_aux01+0.1, loss_rate04)
yl8 <- c(loss_rate02+0.001, loss_rate_aux02+0.1, loss_rate05)
yl9 <- c(loss_rate03+0.001, loss_rate_aux03+0.1, loss_rate06)

loss_rate07 <- project(x, yl7, 30)
loss_rate08 <- project(x, yl8, 30)
loss_rate09 <- project(x, yl9, 30)

#################### Continuidade ######################
yic7 <- c(contin01, contin_aux01, contin04)
yic8 <- c(contin02, contin_aux02, contin05)
yic9 <- c(contin03, contin_aux03, contin06)

contin07 <- project(x, yic7, 30)
contin08 <- project(x, yic8, 30)
contin09 <- project(x, yic9, 30)


