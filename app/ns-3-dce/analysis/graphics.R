mainlabel <- "GMTP - Taxa de Recepção dos Clientes"
xlabel <- "Pacotes Recebidos"
datalabel <- "Taxa de Recepção (B/s)"

rangex <- range(c(99, m), na.rm=T)
rangey <- range(c(10000, 260000), na.rm=T)

colors <- c("blue", "green", "red", "orange", "purple", "brown", "yellow")
desccolors <- c("blue", "yellow")
desc <- c("Tratamento 1\t\t", "Tratamento 2", "Tratamento 3", 
          "Tratamento 4\t\t", "Tratamento 5", "Tratamento 6", 
          "Tratamento 7\t\t", "Tratamento 8", "Tratamento 9",
          "Tratamento 10\t\t", "Tratamento 11", "Tratamento 12")

rep <- c(c("Topologia A\ncom 1 roteador"), 
         c("Topologia A\ncom 3 roteadores"), 
         c("Topologia A\ncom 30 roteadores"), 
         c("Topologia B\n"))

##################### Inst RX by number of clients ########################.
confbase <- c(800, 1800, 2800, 3800, 4800, 5800)
stepc <- 100
confl <- 0.95
confh <- 1.05

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

##################### Inst RX by number of relays ########################
# plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel)
# lines(inst_rate$idx, inst_rate$I, col=colors[1], lwd=1, lty=1)
# lines(inst_rate$idx, inst_rate$IV, col=colors[2], lwd=2, lty=2)
# lines(inst_rate$idx, inst_rate$VII, col=colors[3], lwd=1, lty=3)
# legend("bottomleft", legend = desc[c(1,4,7)], col = colors, lwd = 3, lty=c(3, 2, 1), cex = 1)
# 
# plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel)
# lines(inst_rate$idx, inst_rate$II, col=colors[1], lwd=1, lty=1)
# lines(inst_rate$idx, inst_rate$V, col=colors[2], lwd=2, lty=2)
# lines(inst_rate$idx, inst_rate$VIII, col=colors[3], lwd=1, lty=3)
# legend("bottomleft", legend = desc[c(2,5, 8)], col = colors, lwd = 3, lty=c(3, 2, 1), cex = 1)
# 
# plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel)
# lines(inst_rate$idx, inst_rate$III, col=colors[1], lwd=1, lty=1)
# lines(inst_rate$id)x, inst_rate$VI, col=colors[2], lwd=2, lty=2)
# lines(inst_rate$idx, inst_rate$IX, col=colors[3], lwd=1, lty=3)
# legend("bottomleft", legend = desc[c(3,6, 9)], col = colors, lwd = 3, lty=c(3, 2, 1), cex = 1)


##################### RX Rate ########################
xlabel <- "Número de nós clientes"
rangex <- range(c(1, 12), na.rm=T)
rangey <- range(c(70000, 260000), na.rm=T)
#rangey <- range(c(230000, 260000), na.rm=T)
axis_at <- c(1:12)
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

text(x=2, y=150000, labels=rep[1])
text(x=5, y=150000, labels=rep[2])
text(x=8, y=150000, labels=rep[3])
text(x=11, y=145000, labels=rep[4])
mtext("Tratamentos", side = 3, line = 3)

abline(v = 3.5, lty=3)
abline(v = 6.5, lty=3)
abline(v = 9.5, lty=3)
desc <- c("GMTP-Linux\t\t\t\t\t\t")
legend("bottomleft", legend = desc, col = desccolors, lwd = 3, lty=c(1), cex = 1)


##################### Loss Rate ########################
mainlabel <- "GMTP - Taxa de perda de pacotes"
datalabel <- "Taxa de perda de pacotes (%)"

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

text(x=2, y=4, labels=rep[1])
text(x=5, y=4, labels=rep[2])
text(x=8, y=4, labels=rep[3])
text(x=11, y=4, labels=rep[4])
mtext("Tratamentos", side = 3, line = 3)
abline(v = 3.5, lty=3)
abline(v = 6.5, lty=3)
abline(v = 9.5, lty=3)
legend("topleft", legend = desc, col = desccolors, lwd = 3, lty=c(1), cex = 1)


##################### Continuidade ########################
mainlabel <- "GMTP - Índice de continuidade"
datalabel <- "Índice de continuidade (%)"

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

text(x=2, y=50, labels=rep[1])
text(x=5, y=50, labels=rep[2])
text(x=8, y=50, labels=rep[3])
text(x=11, y=50, labels=rep[4])
mtext("Tratamentos", side = 3, line = 3)
gmtp_orig_contin <- c(99, 98, 96)
gmtp_orig_contin_lc <- c(97, 96, 93)
gmtp_orig_contin_hc <- c(100, 100, 99)

points(gmtp$t[10:12], gmtp_orig_contin_lc, col=colors[2], cex=2, pch= "_")
points(gmtp$t[10:12], gmtp_orig_contin_hc, col=colors[2], cex=2, pch= "_")
segments(gmtp$t[10:12], gmtp_orig_contin_lc, gmtp$t[10:12], gmtp_orig_contin_hc, col=colors[1], lwd=2, lty=1)
lines(gmtp$t[10:12], gmtp_orig_contin, col=colors[2], lwd=2, lty=1)
points(gmtp$t[10:12], gmtp_orig_contin, col=colors[4], lwd=4)

abline(v = 3.5, lty=3)
abline(v = 6.5, lty=3)
abline(v = 9.5, lty=3)

desccolors <- c("blue", "green", "yellow")
desc <- c("GMTP-Linux\t\t\t\t\t\t", "GMTP (SALES, 2014)\t\t\t\t\t\t")
legend("bottomleft", legend = desc, col = desccolors, lwd = 3, lty=c(1, 1), cex = 1)

##################### NDP ########################
mainlabel <- "GMTP - Pacotes de controle"
datalabel <- "Quantidade de pacotes de controle"

rangex <- range(c(10, 12.2), na.rm=T)
rangey <- range(0, max(gmtp$control)+10000, na.rm=T)
kb <- c("KB")
mb <- c("MB")

plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel, xaxt = "n")
axis(1, side=1, at=axis_at[10:12], labels=gmtp$clients[10:12], cex.axis=1)
axis(3, at=axis_at[10:12], cex.axis=1)
points(gmtp$t[10:12], gmtp$control[10:12], col=colors[5], lwd=4)
step <- 0.1
text(x=10.07, y=gmtp$control[10]-2000, labels=round(gmtp$control[10]/1024, digits = 2))
text(x=10.07+step, y=gmtp$control[10]-2000, labels=mb[1])
text(x=11.07, y=gmtp$control[11]-2000, labels=round(gmtp$control[11]/1024, digits = 2))
text(x=11.07+step, y=gmtp$control[11]-2000, labels=mb[1])
text(x=12.07, y=gmtp$control[12]-2000, labels=round(gmtp$control[12]/1024, digits = 2))
text(x=12.07+step, y=gmtp$control[12]-2000, labels=mb[1])
lines(gmtp$t[10:12], gmtp$control[10:12], col=colors[1], lwd=3, lty=1)

gmtp_orig_control <- c(ndp_quantile(1024), ndp_quantile(1700), ndp_quantile(1800))
lines(gmtp$t[10:12], gmtp_orig_control, col=colors[2], lwd=3, lty=1)
points(gmtp$t[10:12], gmtp_orig_control, col=colors[4], lwd=4, lty=1)
text(x=10.07, y=gmtp_orig_control[1]-2000, labels=round(gmtp_orig_control[1]/1024, digits = 2))
text(x=10.07+step, y=gmtp_orig_control[1]-2000, labels=mb[1])
text(x=11.07, y=gmtp_orig_control[2]-2000, labels=round(gmtp_orig_control[2]/1024, digits = 2))
text(x=11.07+step, y=gmtp_orig_control[2]-2000, labels=mb[1])
text(x=12.07, y=gmtp_orig_control[3]-2000, labels=round(gmtp_orig_control[3]/1024, digits = 2))
text(x=12.07+step, y=gmtp_orig_control[3]-2000, labels=mb[1])

#text(x=2, y=53000, labels=rep[1])
#text(x=5, y=53000, labels=rep[2])
#text(x=8, y=53000, labels=rep[3])
#text(x=11, y=51500, labels=rep[4])
mtext("Tratamentos", side = 3, line = 3)
legend("bottomleft", legend = desc[1:2], col = colors, lwd = 3, lty=c(1, 1), cex = 1)


##################### NDP (BARPLOT) ########################
rangex <- range(c(1, 12.7), na.rm=T)
barcolors <- c("red", "aquamarine")
xlabel <- "Número de nós clientes (Tratamento #)"
names <- c("1 (1)", "10 (2)", "20 (3)", 
           "3 (4)", "30 (5)", "60 (6)", 
           "30 (7)", "300 (8)", "600 (9)", 
           "500 (10)", "1500 (11)", "15000 (12)")
data <- rbind(gmtp$control_s, gmtp$control_cli)
midbar <- barplot(as.matrix(data), ylim = rangey, main=mainlabel,xlab=xlabel, ylab=datalabel, col = barcolors, names.arg=names)

text(x=1.9, y=47000, labels=rep[1])
text(x=5.3, y=47000, labels=rep[2])
text(x=9, y=47000, labels=rep[3])
text(x=12.6, y=46500, labels=rep[4])

points(midbar, gmtp$control+2000, cex=2, pch= "_")
points(midbar, gmtp$control-2000, cex=2, pch= "_")
segments(midbar, gmtp$control-2000, midbar, gmtp$control+2000, lwd=2, lty=1)

extra_axis <- seq(0, 71000, 9680)
axis(4, at=extra_axis, labels=paste(round(ndp_len_mb(extra_axis)), mb[1]))
text(midbar, y=gmtp$control+4000, labels=paste(round(ndp_len_mb(gmtp$control), digits = 2), mb[1]))

abline(v = 3.7, lty=3)
abline(v = 7.3, lty=3)
abline(v = 10.9, lty=3)
desc <- c("Pacotes de controle inter-redes\t", "Pacotes de controle local\t")
legend("topleft", legend = desc, fill = barcolors)

##################### NDP Por cliente (BARPLOT) ########################
mainlabel <- "GMTP - Pacotes de controle por cliente"
datalabel <- "Quantidade de pacotes de controle por cliente"
rangex <- range(c(1, 12.7), na.rm=T)
rangey <- range(0, max(gmtp$control/gmtp$clients)+2000, na.rm=T)
barcolors <- c("red", "aquamarine")
xlabel <- "Número de nós clientes (Tratamento #)"
names <- c("1 (1)", "10 (2)", "20 (3)", 
           "3 (4)", "30 (5)", "60 (6)", 
           "30 (7)", "300 (8)", "600 (9)", 
           "500 (10)", "1500 (11)", "15000 (12)")
data <- rbind(gmtp$control_s/gmtp$clients, gmtp$control_cli/gmtp$clients)
midbar <- barplot(as.matrix(data), ylim = rangey, main=mainlabel,xlab=xlabel, ylab=datalabel, col = barcolors, names.arg=names)

text(x=1.9, y=5700, labels=rep[1])
text(x=5.3, y=5700, labels=rep[2])
text(x=9, y=5700, labels=rep[3])
text(x=12.6, y=5700, labels=rep[4])

ic_low <- (gmtp$control/gmtp$clients)-200
for(i in length(ic_low)) {
  if(ic_low[i] < 0){
    ic_low[i] <- 0
  }
}

points(midbar, (gmtp$control/gmtp$clients)+200, cex=2, pch= "_")
points(midbar, ic_low, cex=2, pch= "_")
segments(midbar, ic_low, midbar, (gmtp$control/gmtp$clients)+200, lwd=2, lty=1)

extra_axis2 <- seq(0, max(gmtp$control/gmtp$clients)+2000, 1000)
axis(4, at=extra_axis2, labels=paste(round(ndp_len_kb(extra_axis2)), kb[1]))
text(midbar, y=(gmtp$control/gmtp$clients)+400, labels=paste(round(ndp_len_kb(gmtp$control/gmtp$clients), digits=2), kb[1]))

abline(v = 3.7, lty=3)
abline(v = 7.3, lty=3)
abline(v = 10.9, lty=3)
desc <- c("Pacotes de controle inter-redes\t", "Pacotes de controle local\t")
legend("topleft", legend = desc, fill = barcolors)

###########################################
