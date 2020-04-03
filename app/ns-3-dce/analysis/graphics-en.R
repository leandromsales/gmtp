mainlabel <- "GMTP - Reception Rate"
xlabel <- "Received Packets"
datalabel <- "Reception Rate (B/s)"

colors <- c("blue", "green", "red", "orange", "purple", "brown", "yellow")
desccolors <- c("blue", "yellow")
desc <- c("Treatment 1\t\t", "Treatment 2", "Treatment 3", 
          "Treatment 4\t\t", "Treatment 5", "Treatment 6", 
          "Treatment 7\t\t", "Treatment 8", "Treatment 9",
          "Treatment 10\t\t", "Treatment 11", "Treatment 12")

rep <- c(c("Topology A\nwith 1 router"), 
         c("Topology A\nwith 3 routers"), 
         c("Topology A\nwith 30 routers"), 
         c("Topology B\n"))

##################### Inst RX by number of clients ########################.
confbase <- c(800, 1800, 2800, 3800, 4800, 5800)
stepc <- 100
confl <- 0.95
confh <- 1.05

rangex <- range(c(99, m), na.rm=T)
rangey <- range(c(10000, 260000), na.rm=T)

###### I, II, III ######
plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel)

conf_x <- confbase
points(conf_x, inst_rate$I[conf_x]*confl, col=colors[1], cex=2, pch= "_")
points(conf_x, inst_rate$I[conf_x]*confh, col=colors[1], cex=2, pch= "_")
segments(conf_x, inst_rate$I[conf_x]*confl, conf_x, inst_rate$I[conf_x]*confh, col=colors[1], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$I, col=colors[1], lwd=1, lty=1)

conf_x <- confbase+stepc
points(conf_x, inst_rate$II[conf_x]*confl, col=colors[2], cex=2, pch= "_")
points(conf_x, inst_rate$II[conf_x]*confh, col=colors[2], cex=2, pch= "_")
segments(conf_x, inst_rate$II[conf_x]*confl, conf_x, inst_rate$II[conf_x]*confh, col=colors[2], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$II, col=colors[2], lwd=2, lty=2)

conf_x <- confbase+(2*stepc)
points(conf_x, inst_rate$III[conf_x]*confl, col=colors[3], cex=2, pch= "_")
points(conf_x, inst_rate$III[conf_x]*confh, col=colors[3], cex=2, pch= "_")
segments(conf_x, inst_rate$III[conf_x]*confl, conf_x, inst_rate$III[conf_x]*confh, col=colors[3], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$III, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(1:3)], col = colors, lwd = 3, lty=c(1, 2, 3), cex = 1)

###### IV, V, VI ######
plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel)

conf_x <- confbase
points(conf_x, inst_rate$IV[conf_x]*confl, col=colors[1], cex=2, pch= "_")
points(conf_x, inst_rate$IV[conf_x]*confh, col=colors[1], cex=2, pch= "_")
segments(conf_x, inst_rate$IV[conf_x]*confl, conf_x, inst_rate$IV[conf_x]*confh, col=colors[1], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$IV, col=colors[1], lwd=1, lty=1)

conf_x <- confbase+stepc
points(conf_x, inst_rate$V[conf_x]*confl, col=colors[2], cex=2, pch= "_")
points(conf_x, inst_rate$V[conf_x]*confh, col=colors[2], cex=2, pch= "_")
segments(conf_x, inst_rate$V[conf_x]*confl, conf_x, inst_rate$V[conf_x]*confh, col=colors[2], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$V, col=colors[2], lwd=2, lty=2)

conf_x <- confbase+(2*stepc)
points(conf_x, inst_rate$VI[conf_x]*confl, col=colors[3], cex=2, pch= "_")
points(conf_x, inst_rate$VI[conf_x]*confh, col=colors[3], cex=2, pch= "_")
segments(conf_x, inst_rate$VI[conf_x]*confl, conf_x, inst_rate$VI[conf_x]*confh, col=colors[3], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$VI, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(4:6)], col = colors, lwd = 3, lty=c(1, 2, 3), cex = 1)

###### VII, VIII, IX ######
plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel)

conf_x <- confbase
points(conf_x, inst_rate$VII[conf_x]*confl, col=colors[1], cex=2, pch= "_")
points(conf_x, inst_rate$VII[conf_x]*confh, col=colors[1], cex=2, pch= "_")
segments(conf_x, inst_rate$VII[conf_x]*confl, conf_x, inst_rate$VII[conf_x]*confh, col=colors[1], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$VII, col=colors[1], lwd=1, lty=1)

conf_x <- confbase+stepc
points(conf_x, inst_rate$VIII[conf_x]*confl, col=colors[2], cex=2, pch= "_")
points(conf_x, inst_rate$VIII[conf_x]*confh, col=colors[2], cex=2, pch= "_")
segments(conf_x, inst_rate$VIII[conf_x]*confl, conf_x, inst_rate$VIII[conf_x]*confh, col=colors[2], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$VIII, col=colors[2], lwd=2, lty=2)

conf_x <- confbase+(2*stepc)
points(conf_x, inst_rate$IX[conf_x]*confl, col=colors[3], cex=2, pch= "_")
points(conf_x, inst_rate$IX[conf_x]*confh, col=colors[3], cex=2, pch= "_")
segments(conf_x, inst_rate$IX[conf_x]*confl, conf_x, inst_rate$IX[conf_x]*confh, col=colors[3], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$IX, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(7:9)], col = colors, lwd = 3, lty=c(1, 2, 3), cex = 1)

###### X, XI, XII ######
plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel)

conf_x <- confbase
points(conf_x, inst_rate$X[conf_x]*confl, col=colors[1], cex=2, pch= "_")
points(conf_x, inst_rate$X[conf_x]*confh, col=colors[1], cex=2, pch= "_")
segments(conf_x, inst_rate$X[conf_x]*confl, conf_x, inst_rate$X[conf_x]*confh, col=colors[1], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$X, col=colors[1], lwd=1, lty=1)

conf_x <- confbase+stepc
points(conf_x, inst_rate$XI[conf_x]*confl, col=colors[2], cex=2, pch= "_")
points(conf_x, inst_rate$XI[conf_x]*confh, col=colors[2], cex=2, pch= "_")
segments(conf_x, inst_rate$XI[conf_x]*confl, conf_x, inst_rate$XI[conf_x]*confh, col=colors[2], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$XI, col=colors[2], lwd=2, lty=2)

conf_x <- confbase+(2*stepc)
points(conf_x, inst_rate$XII[conf_x]*confl, col=colors[3], cex=2, pch= "_")
points(conf_x, inst_rate$XII[conf_x]*confh, col=colors[3], cex=2, pch= "_")
segments(conf_x, inst_rate$XII[conf_x]*confl, conf_x, inst_rate$XII[conf_x]*confh, col=colors[3], lwd=2, lty=1)
lines(inst_rate$idx, inst_rate$XII, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(10:12)], col = colors, lwd = 3, lty=c(1, 2, 3), cex = 1)


##################### RX Rate ########################
xlabel <- "Number of Clients"
rangex <- range(c(1, 12), na.rm=T)
rangey <- range(c(70000, 260000), na.rm=T)
#rangey <- range(c(230000, 260000), na.rm=T)
axis_at <- c(1:12)

textpos <- c(2, 5, 8, 11)
linepos <- c(3.5, 6.5, 9.5)

plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel, xaxt="n")
axis(1, side=1, at=axis_at, labels=gmtp$clients, cex.axis=1)
axis(3, at=axis_at, cex.axis=1)
points(gmtp$t, gmtp$rate, col=colors[1], lwd=4)

points(gmtp$t[1:3], rx_lc[1:3], col=colors[1], cex=2, pch= "_")
points(gmtp$t[1:3], rx_hc[1:3], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[1:3], rx_lc[1:3], gmtp$t[1:3], rx_hc[1:3], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[1:3], gmtp$rate[1:3], col=colors[1], lwd=2, lty=1)

points(gmtp$t[4:6], rx_lc[4:6], col=colors[1], cex=2, pch= "_")
points(gmtp$t[4:6], rx_hc[4:6], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[4:6], rx_lc[4:6], gmtp$t[4:6], rx_hc[4:6], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[4:6], gmtp$rate[4:6], col=colors[1], lwd=2, lty=1)

points(gmtp$t[7:9], rx_lc[7:9], col=colors[1], cex=2, pch= "_")
points(gmtp$t[7:9], rx_hc[7:9], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[7:9], rx_lc[7:9], gmtp$t[7:9], rx_hc[7:9], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[7:9], gmtp$rate[7:9], col=colors[1], lwd=2, lty=1)

points(gmtp$t[10:12], rx_lc[10:12], col=colors[1], cex=2, pch= "_")
points(gmtp$t[10:12], rx_hc[10:12], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[10:12], rx_lc[10:12], gmtp$t[10:12], rx_hc[10:12], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[10:12], gmtp$rate[10:12], col=colors[1], lwd=2, lty=1)

text(x=textpos, y=140000, labels=rep)
abline(v=linepos, lty=3)

mtext("Treatments", side = 3, line = 3)
desc <- c("GMTP-Linux\t\t")
legend("bottomleft", legend = desc, col = desccolors, lwd = 3, lty=c(1), cex = 1)


##################### Loss Rate ########################
mainlabel <- "GMTP - Loss Rate"
datalabel <- "Loss rate (%)"

rangey <- range(c(0, max(gmtp$loss)+1), na.rm=T)
plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel, xaxt = "n")
axis(1, side=1, at=axis_at, labels=gmtp$clients, cex.axis=1)
axis(3, at=axis_at, cex.axis=1)
points(gmtp$t, gmtp$loss, col=colors[1], lwd=4)

#points(gmtp$t[1:3], loss_lc[1:3], col=colors[1], cex=2, pch= "_")
#points(gmtp$t[1:3], loss_hc[1:3], col=colors[1], cex=2, pch= "_")
#segments(gmtp$t[1:3], loss_lc[1:3], gmtp$t[1:3], loss_hc[1:3], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[1:3], gmtp$loss[1:3], col=colors[1], lwd=2, lty=1)

points(gmtp$t[4:6], loss_lc[4:6], col=colors[1], cex=2, pch= "_")
points(gmtp$t[4:6], loss_hc[4:6], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[4:6], loss_lc[4:6], gmtp$t[4:6], loss_hc[4:6], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[4:6], gmtp$loss[4:6], col=colors[1], lwd=2, lty=1)

points(gmtp$t[7:9], loss_lc[7:9], col=colors[1], cex=2, pch= "_")
points(gmtp$t[7:9], loss_hc[7:9], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[7:9], loss_lc[7:9], gmtp$t[7:9], loss_hc[7:9], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[7:9], gmtp$loss[7:9], col=colors[1], lwd=2, lty=1)

points(gmtp$t[10:12], loss_lc[10:12], col=colors[1], cex=2, pch= "_")
points(gmtp$t[10:12], loss_hc[10:12], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[10:12], loss_lc[10:12], gmtp$t[10:12], loss_hc[10:12], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[10:12], gmtp$loss[10:12], col=colors[1], lwd=2, lty=1)

text(x=textpos, y=4, labels=rep)
abline(v=linepos, lty=3)

mtext("Treatments", side = 3, line = 3)
legend("topleft", legend = desc, col = desccolors, lwd = 3, lty=c(1), cex = 1)


##################### Continuidade ########################
mainlabel <- "GMTP - Continuity Index"
datalabel <- "Continuity Index (%)"

rangey <- range(c(0, 101), na.rm=T)
plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel, xaxt = "n")
axis(1, side=1, at=axis_at, labels=gmtp$clients, cex.axis=1)
axis(3, at=axis_at, cex.axis=1)
points(gmtp$t, gmtp$contin, col=colors[1], lwd=4)

points(gmtp$t[1:3], contin_lc[1:3], col=colors[1], cex=2, pch= "_")
points(gmtp$t[1:3], contin_hc[1:3], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[1:3], contin_lc[1:3], gmtp$t[1:3], contin_hc[1:3], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[1:3], gmtp$contin[1:3], col=colors[1], lwd=2, lty=1)

points(gmtp$t[4:6], contin_lc[4:6], col=colors[1], cex=2, pch= "_")
points(gmtp$t[4:6], contin_hc[4:6], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[4:6], contin_lc[4:6], gmtp$t[4:6], contin_hc[4:6], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[4:6], gmtp$contin[4:6], col=colors[1], lwd=2, lty=1)

points(gmtp$t[7:9], contin_lc[7:9], col=colors[1], cex=2, pch= "_")
points(gmtp$t[7:9], contin_hc[7:9], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[7:9], contin_lc[7:9], gmtp$t[7:9], contin_hc[7:9], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[7:9], gmtp$contin[7:9], col=colors[1], lwd=2, lty=1)

points(gmtp$t[10:12], contin_lc[10:12], col=colors[1], cex=2, pch= "_")
points(gmtp$t[10:12], contin_hc[10:12], col=colors[1], cex=2, pch= "_")
segments(gmtp$t[10:12], contin_lc[10:12], gmtp$t[10:12], contin_hc[10:12], col=colors[1], lwd=2, lty=1)
lines(gmtp$t[10:12], gmtp$contin[10:12], col=colors[1], lwd=2, lty=1)

text(x=textpos, y=50, labels=rep)
abline(v=linepos, lty=3)

#mtext("Treatments", side = 3, line = 3)
#gmtp_orig_contin <- c(99, 98, 96)
#gmtp_orig_contin_lc <- c(97, 96, 93)
#gmtp_orig_contin_hc <- c(100, 100, 99)

#points(gmtp$t[10:12], gmtp_orig_contin_lc, col=colors[2], cex=2, pch= "_")
#points(gmtp$t[10:12], gmtp_orig_contin_hc, col=colors[2], cex=2, pch= "_")
#segments(gmtp$t[10:12], gmtp_orig_contin_lc, gmtp$t[10:12], gmtp_orig_contin_hc, col=colors[1], lwd=2, lty=1)
#lines(gmtp$t[10:12], gmtp_orig_contin, col=colors[2], lwd=2, lty=1)
#points(gmtp$t[10:12], gmtp_orig_contin, col=colors[4], lwd=4)

#desccolors <- c("blue", "green", "yellow")
#desc <- c("GMTP-Linux\t\t\t\t\t\t", "GMTP (SALES, 2014)\t\t\t\t\t\t")
legend("bottomleft", legend = desc, col = desccolors, lwd = 3, lty=c(1, 1), cex = 1)

##################### NDP (BARPLOT) ########################
#-------- Part 1 ---------
rangex <- range(c(1, 6.3), na.rm=T)
rangey <- range(0, ctrl_hc+10000, na.rm=T)
barcolors <- c("red", "aquamarine")
xlabel <- "Number of clients (Treatment #)"
names <- c("1 (1)", "10 (2)", "20 (3)", 
           "3 (4)", "30 (5)", "60 (6)")
data <- rbind(gmtp$control_s[1:6], gmtp$control_cli[1:6])
midbar <- barplot(as.matrix(data), ylim = rangey, main=mainlabel,xlab=xlabel, ylab=datalabel, col = barcolors, names.arg=names)

points(midbar, ctrl_lc[1:6], cex=2, pch= "_")
points(midbar, ctrl_hc[1:6], cex=2, pch= "_")
segments(midbar, ctrl_lc[1:6], midbar, ctrl_hc[1:6], lwd=2, lty=1)

#extra_axis <- seq(0, 71000, 9680)
#axis(4, at=extra_axis, labels=paste(round(ndp_len_mb(extra_axis)), mb[1]))
text(midbar, y=ctrl_hc[1:6]+2000, labels=paste(round(ndp_len_mb(gmtp$control[1:6]), digits = 2), mb[1]))

bartextpos <- c(1.9, 5.3, 9, 12.6)
barlinepos <- c(3.7, 7.3, 10.9)
text(x=bartextpos, y=46500, labels=rep[1:2])
abline(v=barlinepos, lty=3)

desc <- c("Inter-network control packets\t\t", "Local control packets\t\t")
legend("topleft", legend = desc, fill = barcolors)

#-------- Part 2 ---------
rangex <- range(c(7, 12.3), na.rm=T)
names <- c("30 (7)", "300 (8)", "600 (9)", 
           "500 (10)", "1500 (11)", "15000 (12)")
data <- rbind(gmtp$control_s[7:12], gmtp$control_cli[7:12])
midbar <- barplot(as.matrix(data), ylim = rangey, main=mainlabel,xlab=xlabel, ylab=datalabel, col = barcolors, names.arg=names)

points(midbar, ctrl_lc[7:12], cex=2, pch= "_")
points(midbar, ctrl_hc[7:12], cex=2, pch= "_")
segments(midbar, ctrl_lc[7:12], midbar, ctrl_hc[7:12], lwd=2, lty=1)

#extra_axis <- seq(0, 71000, 9680)
#axis(4, at=extra_axis, labels=paste(round(ndp_len_mb(extra_axis)), mb[1]))
text(midbar, y=ctrl_hc[7:12]+2000, labels=paste(round(ndp_len_mb(gmtp$control[7:12]), digits = 2), mb[1]))

bartextpos <- c(1.9, 5.3, 9, 12.6)
barlinepos <- c(3.7, 7.3, 10.9)
text(x=bartextpos, y=46500, labels=rep[3:4])
abline(v=barlinepos, lty=3)

legend("topleft", legend = desc, fill = barcolors)

##################### NDP Por cliente (BARPLOT) ########################
#-------- Part 1 ---------
mainlabel <- "GMTP - Overhead of Control Messages per Client"
datalabel <- "Overhead of control messages per client"
rangex <- range(c(1, 6.3), na.rm=T)
rangey <- range(0, max((ctrl_hc/gmtp$clients) + 1000), na.rm=T)
names <- c("1 (1)", "10 (2)", "20 (3)", 
           "3 (4)", "30 (5)", "60 (6)")
data <- rbind(gmtp$control_s[1:6]/gmtp$clients[1:6], gmtp$control_cli[1:6]/gmtp$clients[1:6])
midbar <- barplot(as.matrix(data), ylim = rangey, main=mainlabel,xlab=xlabel, ylab=datalabel, col = barcolors, names.arg=names)

ic_low <- (ctrl_lc[1:6]/gmtp$clients[1:6])
for(i in 1:length(ic_low)) {
  if(ic_low[i] < 0) {
    ic_low[i] = 0
  }
}

points(midbar, ic_low, cex=2, pch= "_")
points(midbar, (ctrl_hc[1:6]/gmtp$clients[1:6]), cex=2, pch= "_")
segments(midbar, ic_low, midbar, (ctrl_hc[1:6]/gmtp$clients[1:6]), lwd=2, lty=1)

#extra_axis2 <- seq(0, max(gmtp$control/gmtp$clients)+2000, 1000)
#axis(4, at=extra_axis2, labels=paste(round(ndp_len_kb(extra_axis2)), kb[1]))
text(midbar, y=(ctrl_hc[1:6]/gmtp$clients[1:6])+200, labels=paste(round(ndp_len_kb(gmtp$control[1:6]/gmtp$clients[1:6]), digits=2), kb[1]))

bartextpos <- bartextpos + 0.1
text(x=bartextpos, y=5500, labels=rep[1:2])
abline(v=barlinepos, lty=3)

legend("topright", legend = desc, fill = barcolors)

#-------- Part 2 ---------
rangex <- range(c(7, 12.3), na.rm=T)
names <- c("30 (7)", "300 (8)", "600 (9)", 
           "500 (10)", "1500 (11)", "15000 (12)")
data <- rbind(gmtp$control_s[7:12]/gmtp$clients[7:12], gmtp$control_cli[7:12]/gmtp$clients[7:12])
midbar <- barplot(as.matrix(data), ylim = rangey, main=mainlabel,xlab=xlabel, ylab=datalabel, col = barcolors, names.arg=names)

ic_low <- (ctrl_lc[7:12]/gmtp$clients[7:12])
for(i in 1:length(ic_low)) {
  if(ic_low[i] < 0) {
    ic_low[i] = 0
  }
}

points(midbar, ic_low, cex=2, pch= "_")
points(midbar, (ctrl_hc[7:12]/gmtp$clients[7:12]), cex=2, pch= "_")
segments(midbar, ic_low, midbar, (ctrl_hc[7:12]/gmtp$clients[7:12]), lwd=2, lty=1)

#extra_axis2 <- seq(0, max(gmtp$control/gmtp$clients)+2000, 1000)
#axis(4, at=extra_axis2, labels=paste(round(ndp_len_kb(extra_axis2)), kb[1]))
text(midbar, y=(ctrl_hc[7:12]/gmtp$clients[7:12])+200, labels=paste(round(ndp_len_kb(gmtp$control[7:12]/gmtp$clients[7:12]), digits=2), kb[1]))

bartextpos <- bartextpos + 0.1
text(x=bartextpos, y=5500, labels=rep[3:4])
abline(v=barlinepos, lty=3)

legend("topright", legend = desc, fill = barcolors)

###########################################
