mainlabel <- "GMTP - Taxa de Recepção dos Clientes"
xlabel <- "Pacotes Recebidos"
datalabel <- "Taxa de Recepção (B/s)"

rangex <- range(c(99, 7000), na.rm=T)
rangey <- range(c(30000, 260000), na.rm=T)

colors <- c("blue", "orange", "red", "purple","brown")
desc <- c("Trat. 1", "Trat. 2", "Trat. 3", "Trat. 4", "Trat. 5", "Trat. 6", 
          "Trat. 7", "Trat. 8", "Trat. 9")

##################### Inst RX by number of clients ########################
plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$I, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$II, col=colors[2], lwd=2, lty=2)
lines(inst_rate$idx, inst_rate$III, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(1:3)], col = colors, lwd = 3, lty=c(3, 2, 1), cex = 1)

plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$IV, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$V, col=colors[2], lwd=2, lty=2)
lines(inst_rate$idx, inst_rate$VI, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(4:6)], col = colors, lwd = 3, lty=c(3, 2, 1), cex = 1)

plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$VII, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$VIII, col=colors[2], lwd=2, lty=2)
lines(inst_rate$idx, inst_rate$IX, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(7:9)], col = colors, lwd = 3, lty=c(3, 2, 1), cex = 1)

##################### Inst RX by number of relays ########################
plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$I, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$IV, col=colors[2], lwd=2, lty=2)
lines(inst_rate$idx, inst_rate$VII, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(1,4,7)], col = colors, lwd = 3, lty=c(3, 2, 1), cex = 1)

plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$II, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$V, col=colors[2], lwd=2, lty=2)
lines(inst_rate$idx, inst_rate$VIII, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(2,5, 8)], col = colors, lwd = 3, lty=c(3, 2, 1), cex = 1)

plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$III, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$VI, col=colors[2], lwd=2, lty=2)
lines(inst_rate$idx, inst_rate$IX, col=colors[3], lwd=1, lty=3)
legend("bottomleft", legend = desc[c(3,6, 9)], col = colors, lwd = 3, lty=c(3, 2, 1), cex = 1)


##################### RX Rate ########################
xlabel <- "Número de Clientes"
rangex <- range(c(1, 9), na.rm=T)
rangey <- range(c(min(gmtp$rate), 260000), na.rm=T)
plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel, xaxt = "n")
axis(1, side=1, at=c(1:9), labels=gmtp$clients, cex.axis=1)
axis(3, at=c(1:9), cex.axis=1)
points(gmtp$t, gmtp$rate, col=colors[5], lwd=4, lty=1)
lines(gmtp$t[1:3], gmtp$rate[1:3], col=colors[5], lwd=2, lty=1)
lines(gmtp$t[4:6], gmtp$rate[4:6], col=colors[5], lwd=2, lty=1)
lines(gmtp$t[7:9], gmtp$rate[7:9], col=colors[5], lwd=2, lty=1)

##################### Loss Rate ########################
mainlabel <- "GMTP - Taxa de perda de pacotes"
datalabel <- "Taxa de perda de pacotes (%)"

rangex <- range(c(1, 9), na.rm=T)
rangey <- range(c(0, max(gmtp$loss)), na.rm=T)
plot(rangex, rangey, type="n", main=mainlabel, xlab="Número de Clientes", ylab=datalabel, xaxt = "n")
axis(1, side=1, at=c(1:9), labels=gmtp$clients, cex.axis=1)
axis(3, at=c(1:9), cex.axis=1)
points(gmtp$t, gmtp$loss, col=colors[5], lwd=4, lty=1)
lines(gmtp$t[1:3], gmtp$loss[1:3], col=colors[5], lwd=2, lty=1)
lines(gmtp$t[4:6], gmtp$loss[4:6], col=colors[5], lwd=2, lty=1)
lines(gmtp$t[7:9], gmtp$loss[7:9], col=colors[5], lwd=2, lty=1)
