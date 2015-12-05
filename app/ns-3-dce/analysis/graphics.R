mainlabel <- "GMTP - Taxa de Recepção dos Clientes"
xlabel <- "Pacotes Recebidos"
datalabel <- "Taxa de Recepção (B/s)"

rangex <- range(c(99, m+500), na.rm=T)
rangey <- range(c(150000, 260000), na.rm=T)

colors <- c("orange", "red", "brown", "purple","blue")
desc <- c("I", "II", "III", "IV", "V", "VI")

##################### By number of clients ########################
plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$I, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$II, col=colors[2], lwd=2, lty=2)
legend("bottomleft", legend = desc[c(1,2)], col = colors, lwd = 3, lty=c(2, 1), cex = 1)
abline(lm(inst_rate$I~inst_rate$idx), col="green", lwd=1)

plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$I, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$III, col=colors[2], lwd=2, lty=2)
legend("bottomleft", legend = desc[c(1,3)], col = colors, lwd = 3, lty=c(2, 1), cex = 1)

plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$IV, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$V, col=colors[2], lwd=2, lty=2)
legend("bottomleft", legend = desc[c(4,5)], col = colors, lwd = 3, lty=c(2, 1), cex = 1)

plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$IV, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$VI, col=colors[2], lwd=2, lty=2)
legend("bottomleft", legend = desc[c(4,6)], col = colors, lwd = 3, lty=c(2, 1), cex = 1)

##################### By number of relays ########################
plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$I, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$IV, col=colors[2], lwd=2, lty=2)
legend("bottomleft", legend = desc[c(1,4)], col = colors, lwd = 3, lty=c(2, 1), cex = 1)

plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$II, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$V, col=colors[2], lwd=2, lty=2)
legend("bottomleft", legend = desc[c(2,5)], col = colors, lwd = 3, lty=c(2, 1), cex = 1)

plot(rangex, rangey, type="n", main=mainlabel, xlab=xlabel, ylab=datalabel)
lines(inst_rate$idx, inst_rate$III, col=colors[1], lwd=1, lty=1)
lines(inst_rate$idx, inst_rate$VI, col=colors[2], lwd=2, lty=2)
legend("bottomleft", legend = desc[c(3,6)], col = colors, lwd = 3, lty=c(2, 1), cex = 1)