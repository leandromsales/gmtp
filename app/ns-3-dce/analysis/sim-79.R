source("~/gmtp/app/ns-3-dce/analysis/aux-1.R");
source("~/gmtp/app/ns-3-dce/analysis/aux-2.R");
source("~/gmtp/app/ns-3-dce/analysis/aux-3.R");

poisson <- function(x, y, n) {
  mx <- mean(x)
  my <- mean(y)
  r <- n * (log(my*100)/mx) 
  return (exp(r/100) - 1);  # e^n
}

project <- function(x, y, n, max=0) {
  ajuste <- lm(formula = y~log(x))
  #summary(ajuste)
  #anova(ajuste)
  #confint(ajuste)
  if(max>0) {
    plot(range(c(0, 31)), range(c(0, max)))
    #abline(x, exp(y))
    abline(lm(y~x))
    points(y~x)
  } 
  #predict(ajuste,x0,interval="confidence")
  ret <- predict(ajuste, data.frame(x=n), interval="prediction")
  return (ret)
}

x <- c(1, 2, 3)

############# Inst RX rate ##################
inst_rate_sizes <- c(length(inst_rate_gmtp01$idx), 
                     length(inst_rate_gmtp01$mean),
                     length(inst_rate_gmtp02$mean),
                     length(inst_rate_gmtp03$mean),
                     length(inst_rate_gmtp04$mean),
                     length(inst_rate_gmtp05$mean),
                     length(inst_rate_gmtp06$mean)) # VII, VIII e IX do not count
m <- min(sizes)

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
x <- c(1, 2, 3)
yl7 <- c(loss_rate01+0.001, loss_rate_aux01+0.1, loss_rate04)
yl8 <- c(loss_rate02+0.001, loss_rate_aux02+0.1, loss_rate05)
yl9 <- c(loss_rate03+0.001, loss_rate_aux03+0.1, loss_rate06)

loss_rate07 <- project(x, yl7, 30)
loss_rate08 <- project(x, yl8, 30)
loss_rate09 <- project(x, yl9, 30)
