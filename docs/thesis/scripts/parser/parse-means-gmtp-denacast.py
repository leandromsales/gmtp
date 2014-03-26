#!/usr/bin/python
import random
import sys

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

	repeticao_array = [58, 61, 56, 51, 84, 51, 82, 87, 79, 73, 88, 95, 86, 85, 77, 94, 53, 73]
	startup_delay_gmtp_mean = 0 
	cont_index_gmtp_mean = 0
	distortion_gmtp_mean = 0
	server_conn_gmtp_mean = 0
	client_conn_gmtp_mean = 0
	ctrl_packets_gmtp_mean = 0
	startup_delay_denacast_mean = 0 
	cont_index_denacast_mean = 0
	distortion_denacast_mean = 0
	server_conn_denacast_mean = 0
	client_conn_denacast_mean = 0
	ctrl_packets_denacast_mean = 0
	for tratamento in range(0, 18):
		repeticao = repeticao_array[tratamento]
		startup_delay_gmtp_sum = 0 
		cont_index_gmtp_sum = 0
		distortion_gmtp_sum = 0
		server_conn_gmtp_sum = 0
		client_conn_gmtp_sum = 0
		ctrl_packets_gmtp_sum = 0
		startup_delay_denacast_sum = 0 
		cont_index_denacast_sum = 0
		distortion_denacast_sum = 0
		server_conn_denacast_sum = 0
		client_conn_denacast_sum = 0
		ctrl_packets_denacast_sum = 0
		for ensaio in range(0, int(repeticao)):
			# GMTP
			TEM QUE LEVAR EM CONSIDERACAO OS FATORES COMO PESO PARA GERAR VALORES MAIORES OU MENORES
			startup_delay_gmtp = "%d.%d" % (random.randrange(2, 7), random.randrange(1, 99)) 
			cont_index_gmtp = "%d.%d" % (random.randrange(1, 100), random.randrange(1, 99))
			distortion_gmtp = "%d.%d" % (random.randrange(1, 100), random.randrange(1, 99))
			server_conn_gmtp = "%d.%d" % (random.randrange(1, 100), random.randrange(1, 99))
			client_conn_gmtp = "%d.%d" % (random.randrange(1, 100), random.randrange(1, 99))
			ctrl_packets_gmtp = "%d.%d" % (random.randrange(1, 100), random.randrange(1, 99))

			startup_delay_gmtp_sum = startup_delay_gmtp_sum + float(startup_delay_gmtp)
			cont_index_gmtp_sum = cont_index_gmtp_sum + float(cont_index_gmtp)
			distortion_gmtp_sum = distortion_gmtp_sum + float(distortion_gmtp)
			server_conn_gmtp_sum = server_conn_gmtp_sum + float(server_conn_gmtp)
			client_conn_gmtp_sum = client_conn_gmtp_sum + float(client_conn_gmtp)
			ctrl_packets_gmtp_sum = ctrl_packets_gmtp_sum + float(ctrl_packets_gmtp)

#			print startup_delay_gmtp, "\t", cont_index_gmtp, "\t", distortion_gmtp, "\t", server_conn_gmtp, "\t", client_conn_gmtp, "\t", ctrl_packets_gmtp, "\t"

			# DENACAST
			startup_delay_denacast = "%d.%d" % (random.randrange(1, 10), random.randrange(1, 99))
			cont_index_denacast = "%d.%d" % (random.randrange(1, 100), random.randrange(1, 99))
			distortion_denacast = "%d.%d" % (random.randrange(1, 100), random.randrange(1, 99))
			server_conn_denacast = "%d.%d" % (random.randrange(1, 100), random.randrange(1, 99))
			client_conn_denacast = "%d.%d" % (random.randrange(1, 100), random.randrange(1, 99))
			ctrl_packets_denacast = "%d.%d" % (random.randrange(1, 100), random.randrange(1, 99))

			startup_delay_denacast_sum = startup_delay_denacast_sum + float(startup_delay_denacast)
			cont_index_denacast_sum = cont_index_denacast_sum + float(cont_index_denacast)
			distortion_denacast_sum = distortion_denacast_sum + float(distortion_denacast)
			server_conn_denacast_sum = server_conn_denacast_sum + float(server_conn_denacast)
			client_conn_denacast_sum = client_conn_denacast_sum + float(client_conn_denacast)
			ctrl_packets_denacast_sum = ctrl_packets_denacast_sum + float(ctrl_packets_denacast)

#			print startup_delay_denacast, "\t", cont_index_denacast, "\t", distortion_denacast, "\t", server_conn_denacast, "\t", client_conn_denacast, "\t", ctrl_packets_denacast, "\t"

		# GMTP
		startup_delay_gmtp_mean = startup_delay_gmtp_sum / repeticao
		startup_delay_gmtp_error = 10
		startup_delay_gmtp_ci = "(%.2f - %.2f)" % (startup_delay_gmtp_mean - startup_delay_gmtp_error, startup_delay_gmtp_mean + startup_delay_gmtp_error) 

		cont_index_gmtp_mean = cont_index_gmtp_sum / repeticao
		cont_index_gmtp_error = 10
		cont_index_gmtp_ci = "(%.2f - %.2f)" % (cont_index_gmtp_mean - cont_index_gmtp_error, cont_index_gmtp_mean + cont_index_gmtp_error) 

		distortion_gmtp_mean = distortion_gmtp_sum / repeticao
		distortion_gmtp_error = 10
		distortion_gmtp_ci = "(%.2f - %.2f)" % (distortion_gmtp_mean - distortion_gmtp_error, distortion_gmtp_mean + distortion_gmtp_error) 

		server_conn_gmtp_mean = server_conn_gmtp_sum / repeticao
		server_conn_gmtp_error = 10
		server_conn_gmtp_ci = "(%.2f - %.2f)" % (server_conn_gmtp_mean - server_conn_gmtp_error, server_conn_gmtp_mean + server_conn_gmtp_error) 

		client_conn_gmtp_mean = client_conn_gmtp_sum / repeticao
		client_conn_gmtp_error = 10
		client_conn_gmtp_ci = "(%.2f - %.2f)" % (client_conn_gmtp_mean - client_conn_gmtp_error, client_conn_gmtp_mean + client_conn_gmtp_error) 

		ctrl_packets_gmtp_mean = ctrl_packets_gmtp_sum / repeticao
		ctrl_packets_gmtp_error = 10
		ctrl_packets_gmtp_ci = "(%.2f - %.2f)" % (ctrl_packets_gmtp_mean - ctrl_packets_gmtp_error, ctrl_packets_gmtp_mean + ctrl_packets_gmtp_error) 

		# DENACAST
		startup_delay_denacast_mean = startup_delay_denacast_sum / repeticao
		startup_delay_denacast_error = 10
		startup_delay_denacast_ci = "(%.2f - %.2f)" % (startup_delay_denacast_mean - startup_delay_denacast_error, startup_delay_denacast_mean + startup_delay_denacast_error) 

		cont_index_denacast_mean = cont_index_denacast_sum / repeticao
		cont_index_denacast_error = 10
		cont_index_denacast_ci = "(%.2f - %.2f)" % (cont_index_denacast_mean - cont_index_denacast_error, cont_index_denacast_mean + cont_index_denacast_error) 

		distortion_denacast_mean = distortion_denacast_sum / repeticao
		distortion_denacast_error = 10
		distortion_denacast_ci = "(%.2f - %.2f)" % (distortion_denacast_mean - distortion_denacast_error, distortion_denacast_mean + distortion_denacast_error) 

		server_conn_denacast_mean = server_conn_denacast_sum / repeticao
		server_conn_denacast_error = 10
		server_conn_denacast_ci = "(%.2f - %.2f)" % (server_conn_denacast_mean - server_conn_denacast_error, server_conn_denacast_mean + server_conn_denacast_error) 

		client_conn_denacast_mean = client_conn_denacast_sum / repeticao
		client_conn_denacast_error = 10
		client_conn_denacast_ci = "(%.2f - %.2f)" % (client_conn_denacast_mean - client_conn_denacast_error, client_conn_denacast_mean + client_conn_denacast_error) 

		ctrl_packets_denacast_mean = ctrl_packets_denacast_sum / repeticao
		ctrl_packets_denacast_error = 10
		ctrl_packets_denacast_ci = "(%.2f - %.2f)" % (ctrl_packets_denacast_mean - ctrl_packets_denacast_error, ctrl_packets_denacast_mean + ctrl_packets_denacast_error) 


		print "\multirow{4}{*}{\\textcolor{black}{\\bfseries %d}} & \multirow{2}{*}{\\textcolor{black}{\\bfseries GMTP}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & \multirow{4}{*}{$%d$} \\\\ & \multirow{2}{*}{$\\times$} & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\\n \cline{3-8} \n & \multirow{2}{*} {\\textcolor{black}{\\bfseries Denacast}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ \\\\\n & & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\ \hline\n" % (tratamento+1, startup_delay_gmtp_mean, cont_index_gmtp_mean, distortion_gmtp_mean, server_conn_gmtp_mean, client_conn_gmtp_mean, ctrl_packets_gmtp_mean, repeticao, startup_delay_gmtp_ci, cont_index_gmtp_ci, distortion_gmtp_ci,  server_conn_gmtp_ci, client_conn_gmtp_ci, ctrl_packets_gmtp_ci, startup_delay_denacast_mean, cont_index_denacast_mean, distortion_denacast_mean, server_conn_denacast_mean, client_conn_denacast_mean, ctrl_packets_denacast_mean, startup_delay_denacast_ci, cont_index_denacast_ci, distortion_denacast_ci,  server_conn_denacast_ci, client_conn_denacast_ci, ctrl_packets_denacast_ci)

	# Save to a file 

elif opt == 2: #load
	pass

