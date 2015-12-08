mainlabel <- "GMTP - Taxa de Recepção dos Clientes"
xlabel <- "Pacotes Recebidos"
datalabel <- "Taxa de Recepção (B/s)"

rangex <- range(c(99, m), na.rm=T)
rangey <- range(c(10000, 260000), na.rm=T)

colors <- c("blue", "green", "red", "orange", "purple","brown")
desc <- c("Tratamento 1\t\t", "Tratamento 2", "Tratamento 3", 
          "Tratamento 4\t\t", "Tratamento 5", "Tratamento 6", 
          "Tratamento 7\t\t", "Tratamento 8", "Tratamento 9")

rep <- c(c("1 repassador"), c("3 repassadores"), c("30 repassadores"))

##################### Inst RX by number of clients ########################
plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$I, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$II, col=colors[2], lwd=2, lty=2)
lines(inst_rate$idx, inst_rate$III, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(1:3)], col = colors, lwd = 3, lty=c(1, 2, 3), cex = 1)

plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$IV, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$V, col=colors[2], lwd=2, lty=2)
lines(inst_rate$idx, inst_rate$VI, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(4:6)], col = colors, lwd = 3, lty=c(1, 2, 3), cex = 1)

plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$VII, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$VIII, col=colors[2], lwd=2, lty=2)
lines(inst_rate$idx, inst_rate$IX, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(7:9)], col = colors, lwd = 3, lty=c(1, 2, 3), cex = 1)

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
# lines(inst_rate$idx, inst_rate$VI, col=colors[2], lwd=2, lty=2)
# lines(inst_rate$idx, inst_rate$IX, col=colors[3], lwd=1, lty=3)
# legend("bottomleft", legend = desc[c(3,6, 9)], col = colors, lwd = 3, lty=c(3, 2, 1), cex = 1)


##################### RX Rate ########################
xlabel <- "Número de nós clientes"
rangex <- range(c(1, 9), na.rm=T)
rangey <- range(c(10000, 260000), na.rm=T)
plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel, xaxt="n")
axis(1, side=1, at=c(1:9), labels=gmtp$clients, cex.axis=1)
axis(3, at=c(1:9), cex.axis=1)
points(gmtp$t, gmtp$rate, col=colors[5], lwd=4, lty=1)
lines(gmtp$t[1:3], gmtp$rate[1:3], col=colors[1], lwd=3, lty=1)
lines(gmtp$t[4:6], gmtp$rate[4:6], col=colors[1], lwd=3, lty=1)
lines(gmtp$t[7:9], gmtp$rate[7:9], col=colors[1], lwd=3, lty=1)
text(x=2, y=50000, labels=rep[1])
text(x=5, y=50000, labels=rep[2])
text(x=8, y=50000, labels=rep[3])
mtext("Tratamentos", side = 3, line = 3)

##################### Loss Rate ########################
mainlabel <- "GMTP - Taxa de perda de pacotes"
datalabel <- "Taxa de perda de pacotes (%)"

rangex <- range(c(1, 9), na.rm=T)
rangey <- range(c(0, max(gmtp$loss)), na.rm=T)
plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel, xaxt = "n")
axis(1, side=1, at=c(1:9), labels=gmtp$clients, cex.axis=1)
axis(3, at=c(1:9), cex.axis=1)
points(gmtp$t, gmtp$loss, col=colors[5], lwd=4, lty=1)
lines(gmtp$t[1:3], gmtp$loss[1:3], col=colors[1], lwd=3, lty=1)
lines(gmtp$t[4:6], gmtp$loss[4:6], col=colors[1], lwd=3, lty=1)
lines(gmtp$t[7:9], gmtp$loss[7:9], col=colors[1], lwd=3, lty=1)
text(x=2, y=5, labels=rep[1])
text(x=5, y=5, labels=rep[2])
text(x=8, y=5, labels=rep[3])
mtext("Tratamentos", side = 3, line = 3)

##################### Continuidade ########################
mainlabel <- "GMTP - Índice de continuidade"
datalabel <- "Índice de continuidade (%)"

rangey <- range(c(0, 100), na.rm=T)
plot(rangex, rangey, type="n", xlab=xlabel, ylab=datalabel, xaxt = "n")
axis(1, side=1, at=c(1:9), labels=gmtp$clients, cex.axis=1)
axis(3, at=c(1:9), cex.axis=1)
points(gmtp$t, gmtp$contin, col=colors[5], lwd=4, lty=1)
lines(gmtp$t[1:3], gmtp$contin[1:3], col=colors[1], lwd=3, lty=1)
lines(gmtp$t[4:6], gmtp$contin[4:6], col=colors[1], lwd=3, lty=1)
lines(gmtp$t[7:9], gmtp$contin[7:9], col=colors[1], lwd=3, lty=1)
text(x=2, y=20, labels=rep[1])
text(x=5, y=20, labels=rep[2])
text(x=8, y=20, labels=rep[3])
mtext("Tratamentos", side = 3, line = 3)
