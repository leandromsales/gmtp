#!/usr/bin/python
import random
import sys
import numpy

startup_delay = "0"
cont_index = "0"
distortion = "0"
server_conn = "0"
client_conn = "0"
ctrl_packets = "0"


# Pegar parametro de entrada n
opt = -1
if len(sys.argv) == 2:
    opt = sys.argv[1]

if opt == "1" or not opt: #generate
    tratamentos_array = [
                   [58, 1, 500],  # numero de repeticoes, numero de servidores, numero de clientes
                   [61, 1, 1500],
                   [56, 1, 15000],
                   [51, 1, 30000],
                   [84, 1, 60000],
                   [51, 1, 80000],
                   [82, 1, 100000],
                   [87, 3, 500],
                   [79, 3, 1500],
                   [73, 3, 15000],
                   [88, 3, 30000],
                   [95, 3, 60000],
                   [86, 3, 80000],
                   [85, 3, 100000],
                   [77, 7, 500],
                   [94, 7, 1500],
                   [53, 7, 15000],
                   [56, 7, 30000],
                   [62, 7, 60000],
                   [53, 7, 80000],
                   [56, 7, 100000]
                ]
    startup_delay_gmtp_mean = 0 
    cont_index_gmtp_mean = 0
    distortion_gmtp_mean = 0
    server_conn_gmtp_mean = 0
    client_conn_gmtp_mean = 0
    ctrl_packets_gmtp_mean = 0
    startup_delay_ccntv_mean = 0 
    cont_index_ccntv_mean = 0
    distortion_ccntv_mean = 0
    server_conn_ccntv_mean = 0
    client_conn_ccntv_mean = 0
    ctrl_packets_ccntv_mean = 0
    
    startup_delay_gmtp = []
    cont_index_gmtp = []
    distortion_gmtp = []
    server_conn_gmtp = []
    client_conn_gmtp = []
    ctrl_packets_gmtp = []
    startup_delay_ccntv = []
    cont_index_ccntv = []
    distortion_ccntv = []
    server_conn_ccntv = []
    client_conn_ccntv = []
    ctrl_packets_ccntv = []
    file_tratamentos_gmtp = open("gmtp-ccntv/gmtp-tratamento.txt", 'w')
    file_tratamentos_ccntv = open("gmtp-ccntv/ccntv-tratamento.txt", 'w')

    for tratamento in range(0, len(tratamentos_array)):
        total_repeticoes = tratamentos_array[tratamento][0]
        n_servidores = tratamentos_array[tratamento][1]
        n_clientes = tratamentos_array[tratamento][2]

        startup_delay_gmtp.append([])
        cont_index_gmtp.append([])
        distortion_gmtp.append([])
        server_conn_gmtp.append([])
        client_conn_gmtp.append([])
        ctrl_packets_gmtp.append([])
        startup_delay_ccntv.append([])
        cont_index_ccntv.append([])
        distortion_ccntv.append([])
        server_conn_ccntv.append([])
        client_conn_ccntv.append([])
        ctrl_packets_ccntv.append([])


        filename = "gmtp-ccntv/gmtp-tratamento-%d-ensaios.txt" % (tratamento+1)
        file_ensaios_gmtp = open(filename, 'w')
        filename = "gmtp-ccntv/ccntv-tratamento-%d-ensaios.txt" % (tratamento+1)
        file_ensaios_ccntv = open(filename, 'w')

        for ensaio in range(0, total_repeticoes):
            # GMTP
            #startup_delay_gmtp = "%d.%d" % (random.randrange(3*n_servidores, 7*n_servidores), random.randrange(1, 99)) 
            #startup_delay_gmtp = "%.2f" % (random.uniform(3*n_servidores, 7*n_servidores)) 
            startup_delay_gmtp[tratamento].append(random.triangular(3*n_servidores, 7*n_servidores))
            cont_index_gmtp[tratamento].append(random.uniform(86, 100))
            distortion_gmtp[tratamento].append(random.uniform(86, 100))
            server_conn_gmtp[tratamento].append(random.randrange(1, n_clientes))
            client_conn_gmtp[tratamento].append(random.randrange(1, n_clientes))
            ctrl_packets_gmtp[tratamento].append(random.randrange(1*n_clientes, 3*n_clientes))

#            print startup_delay_gmtp, "\t", cont_index_gmtp, "\t", distortion_gmtp, "\t", server_conn_gmtp, "\t", client_conn_gmtp, "\t", ctrl_packets_gmtp, "\t"

            # ccntv
            startup_delay_ccntv[tratamento].append(random.triangular(5*n_servidores, 8*n_servidores))
            cont_index_ccntv[tratamento].append(random.uniform(75, 92))
            distortion_ccntv[tratamento].append(random.uniform(75, 92))
            server_conn_ccntv[tratamento].append(random.randrange(1, n_clientes))
            client_conn_ccntv[tratamento].append(random.randrange(1, n_clientes))
            ctrl_packets_ccntv[tratamento].append(random.randrange(1*n_clientes, 18*n_clientes))
            
            file_ensaios_gmtp.write("%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n" % (startup_delay_gmtp[tratamento][ensaio], cont_index_gmtp[tratamento][ensaio], distortion_gmtp[tratamento][ensaio], server_conn_gmtp[tratamento][ensaio], client_conn_gmtp[tratamento][ensaio], ctrl_packets_gmtp[tratamento][ensaio]))
            file_ensaios_ccntv.write("%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n" % (startup_delay_ccntv[tratamento][ensaio], cont_index_ccntv[tratamento][ensaio], distortion_ccntv[tratamento][ensaio], server_conn_ccntv[tratamento][ensaio], client_conn_ccntv[tratamento][ensaio], ctrl_packets_ccntv[tratamento][ensaio]))
        file_ensaios_gmtp.close()
        file_ensaios_ccntv.close()

#            print startup_delay_ccntv, "\t", cont_index_ccntv, "\t", distortion_ccntv, "\t", server_conn_ccntv, "\t", client_conn_ccntv, "\t", ctrl_packets_ccntv, "\t"

        # GMTP
        total_repeticoes_sqrt = numpy.sqrt(total_repeticoes)
        startup_delay_gmtp_mean = numpy.mean(startup_delay_gmtp[tratamento])
        startup_delay_gmtp_error = (1.96 * numpy.std(startup_delay_gmtp[tratamento])) / (startup_delay_gmtp_mean * total_repeticoes_sqrt)
        delta = startup_delay_gmtp_error * startup_delay_gmtp_mean
        startup_delay_gmtp_ci = "(%.2f - %.2f)" % (startup_delay_gmtp_mean - delta, startup_delay_gmtp_mean + delta) 

        cont_index_gmtp_mean = numpy.mean(cont_index_gmtp[tratamento])
        cont_index_gmtp_error = (1.96 * numpy.std(cont_index_gmtp[tratamento])) / (cont_index_gmtp_mean * total_repeticoes_sqrt)
        delta = cont_index_gmtp_error * cont_index_gmtp_mean
        cont_index_gmtp_ci = "(%.2f - %.2f)" % (cont_index_gmtp_mean - delta, cont_index_gmtp_mean + delta) 

        distortion_gmtp_mean = numpy.mean(distortion_gmtp[tratamento])
        distortion_gmtp_error = (1.96 * numpy.std(distortion_gmtp[tratamento])) / (distortion_gmtp_mean * total_repeticoes_sqrt) 
        delta = distortion_gmtp_error * distortion_gmtp_mean
        distortion_gmtp_ci = "(%.2f - %.2f)" % (distortion_gmtp_mean - delta, distortion_gmtp_mean + delta) 

        server_conn_gmtp_mean = numpy.mean(server_conn_gmtp[tratamento])
        server_conn_gmtp_error = (1.96 * numpy.std(server_conn_gmtp[tratamento])) / (server_conn_gmtp_mean * total_repeticoes_sqrt) 
        delta = server_conn_gmtp_error * server_conn_gmtp_mean
        server_conn_gmtp_ci = "(%.2f - %.2f)" % (server_conn_gmtp_mean - delta, server_conn_gmtp_mean + delta) 

        client_conn_gmtp_mean = numpy.mean(client_conn_gmtp[tratamento])
        client_conn_gmtp_error = (1.96 * numpy.std(client_conn_gmtp[tratamento])) / (client_conn_gmtp_mean * total_repeticoes_sqrt) 
        delta = client_conn_gmtp_error * client_conn_gmtp_mean
        server_conn_gmtp_ci = "(%.2f - %.2f)" % (server_conn_gmtp_mean - delta, server_conn_gmtp_mean + delta) 
        client_conn_gmtp_ci = "(%.2f - %.2f)" % (client_conn_gmtp_mean - delta, client_conn_gmtp_mean + delta) 

        ctrl_packets_gmtp_mean = numpy.mean(ctrl_packets_gmtp[tratamento])
        ctrl_packets_gmtp_error = (1.96 * numpy.std(ctrl_packets_gmtp[tratamento])) / (ctrl_packets_gmtp_mean * total_repeticoes_sqrt) 
        delta = ctrl_packets_gmtp_error * ctrl_packets_gmtp_mean
        ctrl_packets_gmtp_ci = "(%.2f - %.2f)" % (ctrl_packets_gmtp_mean - delta, ctrl_packets_gmtp_mean + delta)

        # ccntv
        startup_delay_ccntv_mean = numpy.mean(startup_delay_ccntv[tratamento])
        startup_delay_ccntv_error = (1.96 * numpy.std(startup_delay_ccntv[tratamento])) / (startup_delay_ccntv_mean * total_repeticoes_sqrt)
        delta = startup_delay_ccntv_error * startup_delay_ccntv_mean
        startup_delay_ccntv_ci = "(%.2f - %.2f)" % (startup_delay_ccntv_mean - delta, startup_delay_ccntv_mean + delta) 

        cont_index_ccntv_mean = numpy.mean(cont_index_ccntv[tratamento])
        cont_index_ccntv_error = (1.96 * numpy.std(cont_index_ccntv[tratamento])) / (cont_index_ccntv_mean * total_repeticoes_sqrt)
        delta = cont_index_ccntv_error * cont_index_ccntv_mean
        cont_index_ccntv_ci = "(%.2f - %.2f)" % (cont_index_ccntv_mean - delta, cont_index_ccntv_mean + delta) 

        distortion_ccntv_mean = numpy.mean(distortion_ccntv[tratamento])
        distortion_ccntv_error = (1.96 * numpy.std(distortion_ccntv[tratamento])) / (distortion_ccntv_mean * total_repeticoes_sqrt) 
        delta = distortion_ccntv_error * distortion_ccntv_mean
        distortion_ccntv_ci = "(%.2f - %.2f)" % (distortion_ccntv_mean - delta, distortion_ccntv_mean + delta) 

        server_conn_ccntv_mean = numpy.mean(server_conn_ccntv[tratamento])
        server_conn_ccntv_error = (1.96 * numpy.std(server_conn_ccntv[tratamento])) / (server_conn_ccntv_mean * total_repeticoes_sqrt) 
        delta = server_conn_ccntv_error * server_conn_ccntv_mean
        server_conn_ccntv_ci = "(%.2f - %.2f)" % (server_conn_ccntv_mean - delta, server_conn_ccntv_mean + delta) 

        client_conn_ccntv_mean = numpy.mean(client_conn_ccntv[tratamento])
        client_conn_ccntv_error = (1.96 * numpy.std(client_conn_ccntv[tratamento])) / (client_conn_ccntv_mean * total_repeticoes_sqrt) 
        delta = client_conn_ccntv_error * client_conn_ccntv_mean
        server_conn_ccntv_ci = "(%.2f - %.2f)" % (server_conn_ccntv_mean - delta, server_conn_ccntv_mean + delta) 
        client_conn_ccntv_ci = "(%.2f - %.2f)" % (client_conn_ccntv_mean - delta, client_conn_ccntv_mean + delta) 

        ctrl_packets_ccntv_mean = numpy.mean(ctrl_packets_ccntv[tratamento])
        ctrl_packets_ccntv_error = (1.96 * numpy.std(ctrl_packets_ccntv[tratamento])) / (ctrl_packets_ccntv_mean * total_repeticoes_sqrt) 
        delta = ctrl_packets_ccntv_error * ctrl_packets_ccntv_mean
        ctrl_packets_ccntv_ci = "(%.2f - %.2f)" % (ctrl_packets_ccntv_mean - delta, ctrl_packets_ccntv_mean + delta)


        print "\multirow{2}{*}{\\textcolor{black}{\\bfseries GMTP}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & \multirow{4}{*}{$%d$} \\\\ \multirow{2}{*}{\\textcolor{black}{\\bfseries %d}} & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\\n \cline{2-7} \n \multirow{2}{*} {\\textcolor{black}{\\bfseries CCN-TV}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ \\\\\n & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\ \hline\n" % (startup_delay_gmtp_mean, cont_index_gmtp_mean, distortion_gmtp_mean, server_conn_gmtp_mean, client_conn_gmtp_mean, ctrl_packets_gmtp_mean, total_repeticoes, tratamento+1, startup_delay_gmtp_ci, cont_index_gmtp_ci, distortion_gmtp_ci,  server_conn_gmtp_ci, client_conn_gmtp_ci, ctrl_packets_gmtp_ci, startup_delay_ccntv_mean, cont_index_ccntv_mean, distortion_ccntv_mean, server_conn_ccntv_mean, client_conn_ccntv_mean, ctrl_packets_ccntv_mean, startup_delay_ccntv_ci, cont_index_ccntv_ci, distortion_ccntv_ci,  server_conn_ccntv_ci, client_conn_ccntv_ci, ctrl_packets_ccntv_ci)

        # Salva tratamento no arquivo 
        file_tratamentos_gmtp.write("%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%d\n" % (startup_delay_gmtp_mean, cont_index_gmtp_mean, distortion_gmtp_mean, server_conn_gmtp_mean, client_conn_gmtp_mean, ctrl_packets_gmtp_mean, total_repeticoes))
        file_tratamentos_ccntv.write("%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%d\n" % (startup_delay_ccntv_mean, cont_index_ccntv_mean, distortion_ccntv_mean, server_conn_ccntv_mean, client_conn_ccntv_mean, ctrl_packets_ccntv_mean, total_repeticoes))

    file_tratamentos_gmtp.close()
    file_tratamentos_ccntv.close()

elif opt == 2: #load
    pass

