source("sim-1.R");
source("sim-2.R");
source("sim-3.R");
source("sim-4.R");
source("sim-5.R");
source("sim-6.R");

sizes <- c(length(inst_rate_gmtp01$idx), 
           length(inst_rate_gmtp01$mean),
           length(inst_rate_gmtp02$mean),
           length(inst_rate_gmtp03$mean),
           length(inst_rate_gmtp04$mean),
           length(inst_rate_gmtp05$mean),
           length(inst_rate_gmtp06$mean))

summary(sizes)
m <- min(sizes)

inst_rate <- data.frame(idx=inst_rate_gmtp01$idx[c(1:m)],
                                 I=inst_rate_gmtp01$mean[c(1:m)],
                                 II=inst_rate_gmtp02$mean[c(1:m)],
                                 III=inst_rate_gmtp03$mean[c(1:m)],
                                 IV=inst_rate_gmtp04$mean[c(1:m)],
                                 V=inst_rate_gmtp05$mean[c(1:m)],
                                 VI=inst_rate_gmtp06$mean[c(1:m)])

