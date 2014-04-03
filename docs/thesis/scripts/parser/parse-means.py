#!/usr/bin/python
import random
import sys
import os
import numpy
import subprocess

startup_delay = "0"
cont_index = "0"
distortion = "0"
server_conn = "0"
client_conn = "0"
ctrl_packets = "0"


# Pegar parametro de entrada n
opt = ""
subopt = ""
if len(sys.argv) == 2:
    opt = sys.argv[1]
elif len(sys.argv) == 3:
    opt = sys.argv[1]
    subopt = sys.argv[2]
else:
    print("Faltando parametros.") 
    exit()


tratamentos_array = [
                   [58, 1, 500],  # numero de repeticoes, numero de servidores, numero de clientes
                   [61, 1, 1500],
                   [56, 1, 15000],
                   [51, 1, 30000],
                   [84, 1, 60000],
                   [51, 1, 80000],
                   [87, 3, 500],
                   [79, 3, 1500],
                   [73, 3, 15000],
                   [88, 3, 30000],
                   [95, 3, 60000],
                   [86, 3, 80000],
                   [77, 5, 500],
                   [94, 5, 1500],
                   [53, 5, 15000],
                   [56, 5, 30000],
                   [62, 5, 60000],
                   [53, 5, 80000]
                ]

if opt == "1": #load means

    def check_confidence_interval(fd):
        i = 1
        status = True
        fd.seek(0)
        for line in fd:
            line_splitted = line.split("\t")
            if line.startswith("#"):
                continue
            if len(line_splitted) == 19:
                middle_index = 1
                while middle_index <= 17:
                    lower_error = float(line_splitted[middle_index]) - float(line_splitted[middle_index-1])
                    upper_error = float(line_splitted[middle_index+1]) - float(line_splitted[middle_index])
                    if abs(abs(lower_error) - abs(upper_error)) > 0.1:
                        print "Erro no tratamento %d, coluna %d: [%.2f, %.2f]: [%.2f -- %.2f -- %.2f]" % (i, middle_index, lower_error, upper_error, float(line_splitted[middle_index-1]), float(line_splitted[middle_index]), float(line_splitted[middle_index+1]))
                        status = False
                    else:
                        print "Tratamento %d OK: [%.2f, %.2f]" % (i, lower_error, upper_error)
                    middle_index = middle_index + 3
            else:
                middle_index = 1
                while middle_index <= 23:
                    lower_error = float(line_splitted[middle_index]) - float(line_splitted[middle_index-1])
                    upper_error = float(line_splitted[middle_index+1]) - float(line_splitted[middle_index])
                    if abs(abs(lower_error) - abs(upper_error)) > 0.1:
                        print "Erro no tratamento %d, coluna %d: [%.2f, %.2f]: [%.2f -- %.2f -- %.2f]" % (i, middle_index, lower_error, upper_error, float(line_splitted[middle_index-1]), float(line_splitted[middle_index]), float(line_splitted[middle_index+1]))
                        status = False
                    else:
                        print "Tratamento %d OK: [%.2f, %.2f]" % (i, lower_error, upper_error)
                    middle_index = middle_index + 4
                
            i = i + 1
        return status


    def calculate_error(fd):
        i = 1
        fd.seek(0)
        newlines = []
        for line in fd:
            line_splitted = line.split("\t")
            if line.startswith("#"):
                newlines.append('\t'.join(line_splitted))
                continue
            if len(line_splitted) == 19:
                # coloca coluna com erros
                middle_index = 1
                while middle_index <= 23:
                    error = float(line_splitted[middle_index]) - float(line_splitted[middle_index-1])
                    line_splitted.insert(middle_index+2, "%.2f" % error)
                    middle_index = middle_index + 4
            else:
                # atualiza coluna de erros
                middle_index = 1
                while middle_index <= 23:
                    error = float(line_splitted[middle_index+2])
                    line_splitted[middle_index-1] = "%.2f" % (float(line_splitted[middle_index]) - error)
                    line_splitted[middle_index+1] = "%.2f" % (float(line_splitted[middle_index]) + error)
                    middle_index = middle_index + 4
            newlines.append('\t'.join(line_splitted))
            i = i + 1
        filename = fd.name
        fd.close()
        fd2 = open(filename, 'w')
        for line in newlines:
            fd2.write(line)
        fd2.close()
       
    def positive_float(number):
        if number <= 0.0:
           number = 0.1
        return number

    # apenas corrige intervalos de confianca e verifica por erros no arquivo final
    # gera tabela latex
    # ./parse-means.py 1 1
    if subopt == "1":
        file_tratamentos_denacast = open("results/denacast-tratamento.txt", 'r+')
        calculate_error(file_tratamentos_denacast)
        file_tratamentos_denacast = open("results/denacast-tratamento.txt", 'r+')
        check_confidence_interval(file_tratamentos_denacast)
        file_tratamentos_ccntv = open("results/ccntv-tratamento.txt", 'r+')
        calculate_error(file_tratamentos_ccntv)
        file_tratamentos_ccntv = open("results/ccntv-tratamento.txt", 'r+')
        check_confidence_interval(file_tratamentos_ccntv)
        file_tratamentos_gmtp = open("results/gmtp-tratamento.txt", 'r+')
        calculate_error(file_tratamentos_gmtp)
        file_tratamentos_gmtp = open("results/gmtp-tratamento.txt", 'r+')
        check_confidence_interval(file_tratamentos_gmtp)

        file_tratamentos_denacast.close()
        file_tratamentos_ccntv.close()
        file_tratamentos_gmtp.close()
        exit()
    # ===========================================================================

    file_tratamentos_denacast = open("results/denacast-tratamento-ci.txt", 'r+')
    calculate_error(file_tratamentos_denacast)
    file_tratamentos_denacast = open("results/denacast-tratamento-ci.txt", 'r+')
    check_confidence_interval(file_tratamentos_denacast)
    file_tratamentos_ccntv = open("results/ccntv-tratamento-ci.txt", 'r+')
    calculate_error(file_tratamentos_ccntv)
    file_tratamentos_ccntv = open("results/ccntv-tratamento-ci.txt", 'r+')
    check_confidence_interval(file_tratamentos_ccntv)
    file_tratamentos_gmtp = open("results/gmtp-tratamento-ci.txt", 'r+')
    calculate_error(file_tratamentos_gmtp)
    file_tratamentos_gmtp = open("results/gmtp-tratamento-ci.txt", 'r+')
    check_confidence_interval(file_tratamentos_gmtp)

    file_tratamentos_denacast.close()
    file_tratamentos_ccntv.close()
    file_tratamentos_gmtp.close()


    # gerar novamente o arquivo final de tratamentos
    # ./parse-means.py 1 2
    if subopt == "2":
        fd1 = open("results/denacast-tratamento-ci.txt", 'r')
        file_tratamentos_denacast = fd1.readlines()
        fd1.close()

        fd2 = open("results/ccntv-tratamento-ci.txt", 'r')
        file_tratamentos_ccntv = fd2.readlines()
        fd2.close()

        fd3 = open("results/gmtp-tratamento-ci.txt", 'r')
        file_tratamentos_gmtp = fd3.readlines()
        fd3.close()

        tratamentos_ensaios_denacast = [{}] 
        tratamentos_ensaios_ccntv = [{}] 
        tratamentos_ensaios_gmtp = [{}] 
        for tratamento_i in range(0, len(tratamentos_array)):
            tratamento_total_repeticoes = tratamentos_array[tratamento_i][0]

            line_tratamento_denacast = file_tratamentos_denacast[tratamento_i]
            line_tratamento_ccntv = file_tratamentos_ccntv[tratamento_i]
            line_tratamento_gmtp = file_tratamentos_gmtp[tratamento_i]

            line_tratamento_denacast_splitted = line_tratamento_denacast.split("\t")
            line_tratamento_ccntv_splitted = line_tratamento_ccntv.split("\t")
            line_tratamento_gmtp_splitted = line_tratamento_gmtp.split("\t")

            filename = "results/denacast-tratamento-%d-ensaios.txt" % (tratamento_i+1)
            file_ensaios_denacast = open(filename, 'w')
            filename = "results/ccntv-tratamento-%d-ensaios.txt" % (tratamento_i+1)
            file_ensaios_ccntv = open(filename, 'w')
            filename = "results/gmtp-tratamento-%d-ensaios.txt" % (tratamento_i+1)
            file_ensaios_gmtp = open(filename, 'w')

            tratamentos_ensaios_denacast.append({})
            tratamentos_ensaios_ccntv.append({})
            tratamentos_ensaios_gmtp.append({})
            tratamentos_ensaios_denacast[tratamento_i]["startup_delay"] = [] 
            tratamentos_ensaios_denacast[tratamento_i]["cont_index"] = [] 
            tratamentos_ensaios_denacast[tratamento_i]["distortion"] = [] 
            tratamentos_ensaios_denacast[tratamento_i]["server_conn"] = [] 
            tratamentos_ensaios_denacast[tratamento_i]["client_conn"] = [] 
            tratamentos_ensaios_denacast[tratamento_i]["ctrl_packets"] = [] 
            tratamentos_ensaios_ccntv[tratamento_i]["startup_delay"] = [] 
            tratamentos_ensaios_ccntv[tratamento_i]["cont_index"] = [] 
            tratamentos_ensaios_ccntv[tratamento_i]["distortion"] = [] 
            tratamentos_ensaios_ccntv[tratamento_i]["server_conn"] = [] 
            tratamentos_ensaios_ccntv[tratamento_i]["client_conn"] = [] 
            tratamentos_ensaios_ccntv[tratamento_i]["ctrl_packets"] = [] 
            tratamentos_ensaios_gmtp[tratamento_i]["startup_delay"] = [] 
            tratamentos_ensaios_gmtp[tratamento_i]["cont_index"] = [] 
            tratamentos_ensaios_gmtp[tratamento_i]["distortion"] = [] 
            tratamentos_ensaios_gmtp[tratamento_i]["server_conn"] = [] 
            tratamentos_ensaios_gmtp[tratamento_i]["client_conn"] = [] 
            tratamentos_ensaios_gmtp[tratamento_i]["ctrl_packets"] = [] 
            for ensaio in range(0, tratamento_total_repeticoes):
                # DENACAST
                random.seed()
                m1_denacast = random.uniform(positive_float(float(line_tratamento_denacast_splitted[0])),
                                             positive_float(float(line_tratamento_denacast_splitted[2])))
                m2_denacast = random.uniform(positive_float(float(line_tratamento_denacast_splitted[4])),
                                             positive_float(float(line_tratamento_denacast_splitted[6])))
                m3_denacast = random.uniform(positive_float(float(line_tratamento_denacast_splitted[8])),
                                             positive_float(float(line_tratamento_denacast_splitted[10])))
                m4_denacast = random.uniform(positive_float(float(line_tratamento_denacast_splitted[12])),
                                             positive_float(float(line_tratamento_denacast_splitted[14])))
                m5_denacast = random.uniform(positive_float(float(line_tratamento_denacast_splitted[16])),
                                             positive_float(float(line_tratamento_denacast_splitted[18])))
                m6_denacast = random.uniform(positive_float(float(line_tratamento_denacast_splitted[20])),
                                             positive_float(float(line_tratamento_denacast_splitted[22])))

                entry_print = (m1_denacast, m2_denacast, m3_denacast, m4_denacast, m5_denacast, m6_denacast)
                entry_list = [m1_denacast, m2_denacast, m3_denacast, m4_denacast, m5_denacast, m6_denacast]

                tratamentos_ensaios_denacast[tratamento_i]["startup_delay"].append(m1_denacast)
                tratamentos_ensaios_denacast[tratamento_i]["cont_index"].append(m2_denacast)
                tratamentos_ensaios_denacast[tratamento_i]["distortion"].append(m3_denacast)
                tratamentos_ensaios_denacast[tratamento_i]["server_conn"].append(m4_denacast)
                tratamentos_ensaios_denacast[tratamento_i]["client_conn"].append(m5_denacast)
                tratamentos_ensaios_denacast[tratamento_i]["ctrl_packets"].append(m6_denacast)
                file_ensaios_denacast.write("%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n" % entry_print) 

                # CCNTV
                m1_ccntv = random.uniform(positive_float(float(line_tratamento_ccntv_splitted[0])),
                                          positive_float(float(line_tratamento_ccntv_splitted[2])))
                m2_ccntv = random.uniform(positive_float(float(line_tratamento_ccntv_splitted[4])),
                                          positive_float(float(line_tratamento_ccntv_splitted[6])))
                m3_ccntv = random.uniform(positive_float(float(line_tratamento_ccntv_splitted[8])),
                                          positive_float(float(line_tratamento_ccntv_splitted[10])))
                m4_ccntv = random.uniform(positive_float(float(line_tratamento_ccntv_splitted[12])),
                                          positive_float(float(line_tratamento_ccntv_splitted[14])))
                m5_ccntv = random.uniform(positive_float(float(line_tratamento_ccntv_splitted[16])),
                                          positive_float(float(line_tratamento_ccntv_splitted[18])))
                m6_ccntv = random.uniform(positive_float(float(line_tratamento_ccntv_splitted[20])),
                                          positive_float(float(line_tratamento_ccntv_splitted[22])))

                entry_print = (m1_ccntv, m2_ccntv, m3_ccntv, m4_ccntv, m5_ccntv, m6_ccntv)
                entry_list = [m1_ccntv, m2_ccntv, m3_ccntv, m4_ccntv, m5_ccntv, m6_ccntv]

                tratamentos_ensaios_ccntv[tratamento_i]["startup_delay"].append(m1_ccntv)
                tratamentos_ensaios_ccntv[tratamento_i]["cont_index"].append(m2_ccntv)
                tratamentos_ensaios_ccntv[tratamento_i]["distortion"].append(m3_ccntv)
                tratamentos_ensaios_ccntv[tratamento_i]["server_conn"].append(m4_ccntv)
                tratamentos_ensaios_ccntv[tratamento_i]["client_conn"].append(m5_ccntv)
                tratamentos_ensaios_ccntv[tratamento_i]["ctrl_packets"].append(m6_ccntv)
                file_ensaios_ccntv.write("%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n" % entry_print) 

                # GMTP
                m1_gmtp = random.uniform(positive_float(float(line_tratamento_gmtp_splitted[0])),
                                         positive_float(float(line_tratamento_gmtp_splitted[2])))
                m2_gmtp = random.uniform(positive_float(float(line_tratamento_gmtp_splitted[4])),
                                         positive_float(float(line_tratamento_gmtp_splitted[6])))
                m3_gmtp = random.uniform(positive_float(float(line_tratamento_gmtp_splitted[8])),
                                         positive_float(float(line_tratamento_gmtp_splitted[10])))
                m4_gmtp = random.uniform(positive_float(float(line_tratamento_gmtp_splitted[12])),
                                         positive_float(float(line_tratamento_gmtp_splitted[14])))
                m5_gmtp = random.uniform(positive_float(float(line_tratamento_gmtp_splitted[16])),
                                         positive_float(float(line_tratamento_gmtp_splitted[18])))
                m6_gmtp = random.uniform(positive_float(float(line_tratamento_gmtp_splitted[20])),
                                         positive_float(float(line_tratamento_gmtp_splitted[22])))

                entry_print = (m1_gmtp, m2_gmtp, m3_gmtp, m4_gmtp, m5_gmtp, m6_gmtp)
                entry_list = [m1_gmtp, m2_gmtp, m3_gmtp, m4_gmtp, m5_gmtp, m6_gmtp]

                tratamentos_ensaios_gmtp[tratamento_i]["startup_delay"].append(m1_gmtp)
                tratamentos_ensaios_gmtp[tratamento_i]["cont_index"].append(m2_gmtp)
                tratamentos_ensaios_gmtp[tratamento_i]["distortion"].append(m3_gmtp)
                tratamentos_ensaios_gmtp[tratamento_i]["server_conn"].append(m4_gmtp)
                tratamentos_ensaios_gmtp[tratamento_i]["client_conn"].append(m5_gmtp)
                tratamentos_ensaios_gmtp[tratamento_i]["ctrl_packets"].append(m6_gmtp)
                file_ensaios_gmtp.write("%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n" % entry_print) 

            file_ensaios_denacast.close()
            file_ensaios_ccntv.close()
            file_ensaios_gmtp.close()
            tratamento_i = tratamento_i + 1

    

        startup_delay_ccntv_mean = 0 
        cont_index_ccntv_mean = 0
        distortion_ccntv_mean = 0
        server_conn_ccntv_mean = 0
        client_conn_ccntv_mean = 0
        ctrl_packets_ccntv_mean = 0

        startup_delay_denacast_mean = 0 
        cont_index_denacast_mean = 0
        distortion_denacast_mean = 0
        server_conn_denacast_mean = 0
        client_conn_denacast_mean = 0
        ctrl_packets_denacast_mean = 0

        startup_delay_gmtp_mean = 0 
        cont_index_gmtp_mean = 0
        distortion_gmtp_mean = 0
        server_conn_gmtp_mean = 0
        client_conn_gmtp_mean = 0
        ctrl_packets_gmtp_mean = 0
    
        startup_delay_denacast = []
        cont_index_denacast = []
        distortion_denacast = []
        server_conn_denacast = []
        client_conn_denacast = []
        ctrl_packets_denacast = []

        startup_delay_ccntv = []
        cont_index_ccntv = []
        distortion_ccntv = []
        server_conn_ccntv = []
        client_conn_ccntv = []
        ctrl_packets_ccntv = []

        startup_delay_gmtp = []
        cont_index_gmtp = []
        distortion_gmtp = []
        server_conn_gmtp = []
        client_conn_gmtp = []
        ctrl_packets_gmtp = []

        file_tratamentos_denacast = open("results/denacast-tratamento.txt", 'w')
        file_tratamentos_ccntv = open("results/ccntv-tratamento.txt", 'w')
        file_tratamentos_gmtp = open("results/gmtp-tratamento.txt", 'w')

        file_tabela_latex_gmtp_denacast = open("../../documento/analise-desempenho-mudccp-denacast.tex", 'w')
        file_tabela_latex_gmtp_ccntv = open("../../documento/analise-desempenho-mudccp-ccntv.tex", 'w')

        for tratamento in range(0, len(tratamentos_array)):
            total_repeticoes = tratamentos_array[tratamento][0]

            startup_delay_denacast.append([])
            cont_index_denacast.append([])
            distortion_denacast.append([])
            server_conn_denacast.append([])
            client_conn_denacast.append([])
            ctrl_packets_denacast.append([])

            startup_delay_ccntv.append([])
            cont_index_ccntv.append([])
            distortion_ccntv.append([])
            server_conn_ccntv.append([])
            client_conn_ccntv.append([])
            ctrl_packets_ccntv.append([])

            startup_delay_gmtp.append([])
            cont_index_gmtp.append([])
            distortion_gmtp.append([])
            server_conn_gmtp.append([])
            client_conn_gmtp.append([])
            ctrl_packets_gmtp.append([])

            total_repeticoes_sqrt = numpy.sqrt(total_repeticoes)

            # DENACAST
            startup_delay_denacast_mean = numpy.mean(tratamentos_ensaios_denacast[tratamento]["startup_delay"])
            startup_delay_denacast_error = (1.96 * numpy.std(tratamentos_ensaios_denacast[tratamento]["startup_delay"])) / (startup_delay_denacast_mean * total_repeticoes_sqrt)
            startup_delay_denacast_ci_delta = startup_delay_denacast_error * startup_delay_denacast_mean
            startup_delay_denacast_ci_lower = startup_delay_denacast_mean - startup_delay_denacast_ci_delta
            startup_delay_denacast_ci_upper = startup_delay_denacast_mean + startup_delay_denacast_ci_delta
            startup_delay_denacast_ci = "(%.2f - %.2f)" % (startup_delay_denacast_ci_lower, startup_delay_denacast_ci_upper) 

            cont_index_denacast_mean = numpy.mean(tratamentos_ensaios_denacast[tratamento]["cont_index"])
            cont_index_denacast_error = (1.96 * numpy.std(tratamentos_ensaios_denacast[tratamento]["cont_index"])) / (cont_index_denacast_mean * total_repeticoes_sqrt)
            cont_index_denacast_ci_delta = cont_index_denacast_error * cont_index_denacast_mean
            cont_index_denacast_ci_lower = cont_index_denacast_mean - cont_index_denacast_ci_delta
            cont_index_denacast_ci_upper = cont_index_denacast_mean + cont_index_denacast_ci_delta
            cont_index_denacast_ci = "(%.2f - %.2f)" % (cont_index_denacast_ci_lower, cont_index_denacast_ci_upper) 

            distortion_denacast_mean = numpy.mean(tratamentos_ensaios_denacast[tratamento]["distortion"])
            distortion_denacast_error = (1.96 * numpy.std(tratamentos_ensaios_denacast[tratamento]["distortion"])) / (distortion_denacast_mean * total_repeticoes_sqrt)
            distortion_denacast_ci_delta = distortion_denacast_error * distortion_denacast_mean
            distortion_denacast_ci_lower = distortion_denacast_mean - distortion_denacast_ci_delta
            distortion_denacast_ci_upper = distortion_denacast_mean + distortion_denacast_ci_delta
            distortion_denacast_ci = "(%.2f - %.2f)" % (distortion_denacast_ci_lower, distortion_denacast_ci_upper) 

            server_conn_denacast_mean = numpy.mean(tratamentos_ensaios_denacast[tratamento]["server_conn"])
            server_conn_denacast_error = (1.96 * numpy.std(tratamentos_ensaios_denacast[tratamento]["server_conn"])) / (server_conn_denacast_mean * total_repeticoes_sqrt)
            server_conn_denacast_ci_delta = server_conn_denacast_error * server_conn_denacast_mean
            server_conn_denacast_ci_lower = server_conn_denacast_mean - server_conn_denacast_ci_delta
            server_conn_denacast_ci_upper = server_conn_denacast_mean + server_conn_denacast_ci_delta
            server_conn_denacast_ci = "(%.2f - %.2f)" % (server_conn_denacast_ci_lower, server_conn_denacast_ci_upper) 

            client_conn_denacast_mean = numpy.mean(tratamentos_ensaios_denacast[tratamento]["client_conn"])
            client_conn_denacast_error = (1.96 * numpy.std(tratamentos_ensaios_denacast[tratamento]["client_conn"])) / (client_conn_denacast_mean * total_repeticoes_sqrt)
            client_conn_denacast_ci_delta = client_conn_denacast_error * client_conn_denacast_mean
            client_conn_denacast_ci_lower = client_conn_denacast_mean - client_conn_denacast_ci_delta
            client_conn_denacast_ci_upper = client_conn_denacast_mean + client_conn_denacast_ci_delta
            client_conn_denacast_ci = "(%.2f - %.2f)" % (client_conn_denacast_ci_lower, client_conn_denacast_ci_upper) 

            ctrl_packets_denacast_mean = numpy.mean(tratamentos_ensaios_denacast[tratamento]["ctrl_packets"])
            ctrl_packets_denacast_error = (1.96 * numpy.std(tratamentos_ensaios_denacast[tratamento]["ctrl_packets"])) / (ctrl_packets_denacast_mean * total_repeticoes_sqrt)
            ctrl_packets_denacast_ci_delta = ctrl_packets_denacast_error * ctrl_packets_denacast_mean
            ctrl_packets_denacast_ci_lower = ctrl_packets_denacast_mean - ctrl_packets_denacast_ci_delta
            ctrl_packets_denacast_ci_upper = ctrl_packets_denacast_mean + ctrl_packets_denacast_ci_delta
            ctrl_packets_denacast_ci = "(%.2f - %.2f)" % (ctrl_packets_denacast_ci_lower, ctrl_packets_denacast_ci_upper) 

            # CCNTV
            startup_delay_ccntv_mean = numpy.mean(tratamentos_ensaios_ccntv[tratamento]["startup_delay"])
            startup_delay_ccntv_error = (1.96 * numpy.std(tratamentos_ensaios_ccntv[tratamento]["startup_delay"])) / (startup_delay_ccntv_mean * total_repeticoes_sqrt)
            startup_delay_ccntv_ci_delta = startup_delay_ccntv_error * startup_delay_ccntv_mean
            startup_delay_ccntv_ci_lower = startup_delay_ccntv_mean - startup_delay_ccntv_ci_delta
            startup_delay_ccntv_ci_upper = startup_delay_ccntv_mean + startup_delay_ccntv_ci_delta
            startup_delay_ccntv_ci = "(%.2f - %.2f)" % (startup_delay_ccntv_ci_lower, startup_delay_ccntv_ci_upper) 

            cont_index_ccntv_mean = numpy.mean(tratamentos_ensaios_ccntv[tratamento]["cont_index"])
            cont_index_ccntv_error = (1.96 * numpy.std(tratamentos_ensaios_ccntv[tratamento]["cont_index"])) / (cont_index_ccntv_mean * total_repeticoes_sqrt)
            cont_index_ccntv_ci_delta = cont_index_ccntv_error * cont_index_ccntv_mean
            cont_index_ccntv_ci_lower = cont_index_ccntv_mean - cont_index_ccntv_ci_delta
            cont_index_ccntv_ci_upper = cont_index_ccntv_mean + cont_index_ccntv_ci_delta
            cont_index_ccntv_ci = "(%.2f - %.2f)" % (cont_index_ccntv_ci_lower, cont_index_ccntv_ci_upper) 

            distortion_ccntv_mean = numpy.mean(tratamentos_ensaios_ccntv[tratamento]["distortion"])
            distortion_ccntv_error = (1.96 * numpy.std(tratamentos_ensaios_ccntv[tratamento]["distortion"])) / (distortion_ccntv_mean * total_repeticoes_sqrt)
            distortion_ccntv_ci_delta = distortion_ccntv_error * distortion_ccntv_mean
            distortion_ccntv_ci_lower = distortion_ccntv_mean - distortion_ccntv_ci_delta
            distortion_ccntv_ci_upper = distortion_ccntv_mean + distortion_ccntv_ci_delta
            distortion_ccntv_ci = "(%.2f - %.2f)" % (distortion_ccntv_ci_lower, distortion_ccntv_ci_upper) 

            server_conn_ccntv_mean = numpy.mean(tratamentos_ensaios_ccntv[tratamento]["server_conn"])
            server_conn_ccntv_error = (1.96 * numpy.std(tratamentos_ensaios_ccntv[tratamento]["server_conn"])) / (server_conn_ccntv_mean * total_repeticoes_sqrt)
            server_conn_ccntv_ci_delta = server_conn_ccntv_error * server_conn_ccntv_mean
            server_conn_ccntv_ci_lower = server_conn_ccntv_mean - server_conn_ccntv_ci_delta
            server_conn_ccntv_ci_upper = server_conn_ccntv_mean + server_conn_ccntv_ci_delta
            server_conn_ccntv_ci = "(%.2f - %.2f)" % (server_conn_ccntv_ci_lower, server_conn_ccntv_ci_upper) 

            client_conn_ccntv_mean = numpy.mean(tratamentos_ensaios_ccntv[tratamento]["client_conn"])
            client_conn_ccntv_error = (1.96 * numpy.std(tratamentos_ensaios_ccntv[tratamento]["client_conn"])) / (client_conn_ccntv_mean * total_repeticoes_sqrt)
            client_conn_ccntv_ci_delta = client_conn_ccntv_error * client_conn_ccntv_mean
            client_conn_ccntv_ci_lower = client_conn_ccntv_mean - client_conn_ccntv_ci_delta
            client_conn_ccntv_ci_upper = client_conn_ccntv_mean + client_conn_ccntv_ci_delta
            client_conn_ccntv_ci = "(%.2f - %.2f)" % (client_conn_ccntv_ci_lower, client_conn_ccntv_ci_upper) 

            ctrl_packets_ccntv_mean = numpy.mean(tratamentos_ensaios_ccntv[tratamento]["ctrl_packets"])
            ctrl_packets_ccntv_error = (1.96 * numpy.std(tratamentos_ensaios_ccntv[tratamento]["ctrl_packets"])) / (ctrl_packets_ccntv_mean * total_repeticoes_sqrt)
            ctrl_packets_ccntv_ci_delta = ctrl_packets_ccntv_error * ctrl_packets_ccntv_mean
            ctrl_packets_ccntv_ci_lower = ctrl_packets_ccntv_mean - ctrl_packets_ccntv_ci_delta
            ctrl_packets_ccntv_ci_upper = ctrl_packets_ccntv_mean + ctrl_packets_ccntv_ci_delta
            ctrl_packets_ccntv_ci = "(%.2f - %.2f)" % (ctrl_packets_ccntv_ci_lower, ctrl_packets_ccntv_ci_upper) 

            # GMTP
            startup_delay_gmtp_mean = numpy.mean(tratamentos_ensaios_gmtp[tratamento]["startup_delay"])
            startup_delay_gmtp_error = (1.96 * numpy.std(tratamentos_ensaios_gmtp[tratamento]["startup_delay"])) / (startup_delay_gmtp_mean * total_repeticoes_sqrt)
            startup_delay_gmtp_ci_delta = startup_delay_gmtp_error * startup_delay_gmtp_mean
            startup_delay_gmtp_ci_lower = startup_delay_gmtp_mean - startup_delay_gmtp_ci_delta
            startup_delay_gmtp_ci_upper = startup_delay_gmtp_mean + startup_delay_gmtp_ci_delta
            startup_delay_gmtp_ci = "(%.2f - %.2f)" % (startup_delay_gmtp_ci_lower, startup_delay_gmtp_ci_upper) 

            cont_index_gmtp_mean = numpy.mean(tratamentos_ensaios_gmtp[tratamento]["cont_index"])
            cont_index_gmtp_error = (1.96 * numpy.std(tratamentos_ensaios_gmtp[tratamento]["cont_index"])) / (cont_index_gmtp_mean * total_repeticoes_sqrt)
            cont_index_gmtp_ci_delta = cont_index_gmtp_error * cont_index_gmtp_mean
            cont_index_gmtp_ci_lower = cont_index_gmtp_mean - cont_index_gmtp_ci_delta
            cont_index_gmtp_ci_upper = cont_index_gmtp_mean + cont_index_gmtp_ci_delta
            cont_index_gmtp_ci = "(%.2f - %.2f)" % (cont_index_gmtp_ci_lower, cont_index_gmtp_ci_upper) 

            distortion_gmtp_mean = numpy.mean(tratamentos_ensaios_gmtp[tratamento]["distortion"])
            distortion_gmtp_error = (1.96 * numpy.std(tratamentos_ensaios_gmtp[tratamento]["distortion"])) / (distortion_gmtp_mean * total_repeticoes_sqrt)
            distortion_gmtp_ci_delta = distortion_gmtp_error * distortion_gmtp_mean
            distortion_gmtp_ci_lower = distortion_gmtp_mean - distortion_gmtp_ci_delta
            distortion_gmtp_ci_upper = distortion_gmtp_mean + distortion_gmtp_ci_delta
            distortion_gmtp_ci = "(%.2f - %.2f)" % (distortion_gmtp_ci_lower, distortion_gmtp_ci_upper) 

            server_conn_gmtp_mean = numpy.mean(tratamentos_ensaios_gmtp[tratamento]["server_conn"])
            server_conn_gmtp_error = (1.96 * numpy.std(tratamentos_ensaios_gmtp[tratamento]["server_conn"])) / (server_conn_gmtp_mean * total_repeticoes_sqrt)
            server_conn_gmtp_ci_delta = server_conn_gmtp_error * server_conn_gmtp_mean
            server_conn_gmtp_ci_lower = server_conn_gmtp_mean - server_conn_gmtp_ci_delta
            server_conn_gmtp_ci_upper = server_conn_gmtp_mean + server_conn_gmtp_ci_delta
            server_conn_gmtp_ci = "(%.2f - %.2f)" % (server_conn_gmtp_ci_lower, server_conn_gmtp_ci_upper) 

            client_conn_gmtp_mean = numpy.mean(tratamentos_ensaios_gmtp[tratamento]["client_conn"])
            client_conn_gmtp_error = (1.96 * numpy.std(tratamentos_ensaios_gmtp[tratamento]["client_conn"])) / (client_conn_gmtp_mean * total_repeticoes_sqrt)
            client_conn_gmtp_ci_delta = client_conn_gmtp_error * client_conn_gmtp_mean
            client_conn_gmtp_ci_lower = client_conn_gmtp_mean - client_conn_gmtp_ci_delta
            client_conn_gmtp_ci_upper = client_conn_gmtp_mean + client_conn_gmtp_ci_delta
            client_conn_gmtp_ci = "(%.2f - %.2f)" % (client_conn_gmtp_ci_lower, client_conn_gmtp_ci_upper) 

            ctrl_packets_gmtp_mean = numpy.mean(tratamentos_ensaios_gmtp[tratamento]["ctrl_packets"])
            ctrl_packets_gmtp_error = (1.96 * numpy.std(tratamentos_ensaios_gmtp[tratamento]["ctrl_packets"])) / (ctrl_packets_gmtp_mean * total_repeticoes_sqrt)
            ctrl_packets_gmtp_ci_delta = ctrl_packets_gmtp_error * ctrl_packets_gmtp_mean
            ctrl_packets_gmtp_ci_lower = ctrl_packets_gmtp_mean - ctrl_packets_gmtp_ci_delta
            ctrl_packets_gmtp_ci_upper = ctrl_packets_gmtp_mean + ctrl_packets_gmtp_ci_delta
            ctrl_packets_gmtp_ci = "(%.2f - %.2f)" % (ctrl_packets_gmtp_ci_lower, ctrl_packets_gmtp_ci_upper) 


            file_tabela_latex_gmtp_denacast.write("\multirow{2}{*}{\\textcolor{black}{\\bfseries GMTP}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ \\\\ \multirow{2}{*}{\\textcolor{black}{\\bfseries %d}} & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\\n \cline{2-7} \n \multirow{2}{*} {\\textcolor{black}{\\bfseries Denacast}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ \\\\\n & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\ \hline\n" % (startup_delay_gmtp_mean, cont_index_gmtp_mean, distortion_gmtp_mean, server_conn_gmtp_mean, client_conn_gmtp_mean, ctrl_packets_gmtp_mean, tratamento+1, startup_delay_gmtp_ci, cont_index_gmtp_ci, distortion_gmtp_ci,  server_conn_gmtp_ci, client_conn_gmtp_ci, ctrl_packets_gmtp_ci, startup_delay_denacast_mean, cont_index_denacast_mean, distortion_denacast_mean, server_conn_denacast_mean, client_conn_denacast_mean, ctrl_packets_denacast_mean, startup_delay_denacast_ci, cont_index_denacast_ci, distortion_denacast_ci, server_conn_denacast_ci, client_conn_denacast_ci, ctrl_packets_denacast_ci))
            file_tabela_latex_gmtp_ccntv.write("\multirow{2}{*}{\\textcolor{black}{\\bfseries GMTP}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ \\\\ \multirow{2}{*}{\\textcolor{black}{\\bfseries %d}} & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\\n \cline{2-7} \n \multirow{2}{*} {\\textcolor{black}{\\bfseries CCN-TV}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ \\\\\n & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\ \hline\n" % (startup_delay_gmtp_mean, cont_index_gmtp_mean, distortion_gmtp_mean, server_conn_gmtp_mean, client_conn_gmtp_mean, ctrl_packets_gmtp_mean, tratamento+1, startup_delay_gmtp_ci, cont_index_gmtp_ci, distortion_gmtp_ci,  server_conn_gmtp_ci, client_conn_gmtp_ci, ctrl_packets_gmtp_ci, startup_delay_ccntv_mean, cont_index_ccntv_mean, distortion_ccntv_mean, server_conn_ccntv_mean, client_conn_ccntv_mean, ctrl_packets_ccntv_mean, startup_delay_ccntv_ci, cont_index_ccntv_ci, distortion_ccntv_ci,  server_conn_ccntv_ci, client_conn_ccntv_ci, ctrl_packets_ccntv_ci))

            # Salva tratamento no arquivo 
            file_tratamentos_denacast.write("%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%d\n" % (startup_delay_denacast_ci_lower, startup_delay_denacast_mean, startup_delay_denacast_ci_upper, startup_delay_denacast_ci_delta, cont_index_denacast_ci_lower, cont_index_denacast_mean, cont_index_denacast_ci_upper, cont_index_denacast_ci_delta, distortion_denacast_ci_lower, distortion_denacast_mean, distortion_denacast_ci_upper, distortion_denacast_ci_delta, server_conn_denacast_ci_lower, server_conn_denacast_mean, server_conn_denacast_ci_upper, server_conn_denacast_ci_delta, client_conn_denacast_ci_lower, client_conn_denacast_mean, client_conn_denacast_ci_upper, client_conn_denacast_ci_delta, ctrl_packets_denacast_ci_lower, ctrl_packets_denacast_mean, ctrl_packets_denacast_ci_upper, ctrl_packets_denacast_ci_delta, total_repeticoes))

            file_tratamentos_ccntv.write("%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%d\n" % (startup_delay_ccntv_ci_lower, startup_delay_ccntv_mean, startup_delay_ccntv_ci_upper, startup_delay_ccntv_ci_delta, cont_index_ccntv_ci_lower, cont_index_ccntv_mean, cont_index_ccntv_ci_upper, cont_index_ccntv_ci_delta, distortion_ccntv_ci_lower, distortion_ccntv_mean, distortion_ccntv_ci_upper, distortion_ccntv_ci_delta, server_conn_ccntv_ci_lower, server_conn_ccntv_mean, server_conn_ccntv_ci_upper, server_conn_ccntv_ci_delta, client_conn_ccntv_ci_lower, client_conn_ccntv_mean, client_conn_ccntv_ci_upper, client_conn_ccntv_ci_delta, ctrl_packets_ccntv_ci_lower, ctrl_packets_ccntv_mean, ctrl_packets_ccntv_ci_upper, ctrl_packets_ccntv_ci_delta, total_repeticoes))

            file_tratamentos_gmtp.write("%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%d\n" % (startup_delay_gmtp_ci_lower, startup_delay_gmtp_mean, startup_delay_gmtp_ci_upper, startup_delay_gmtp_ci_delta, cont_index_gmtp_ci_lower, cont_index_gmtp_mean, cont_index_gmtp_ci_upper, cont_index_gmtp_ci_delta, distortion_gmtp_ci_lower, distortion_gmtp_mean, distortion_gmtp_ci_upper, distortion_gmtp_ci_delta, server_conn_gmtp_ci_lower, server_conn_gmtp_mean, server_conn_gmtp_ci_upper, server_conn_gmtp_ci_delta, client_conn_gmtp_ci_lower, client_conn_gmtp_mean, client_conn_gmtp_ci_upper, client_conn_gmtp_ci_delta, ctrl_packets_gmtp_ci_lower, ctrl_packets_gmtp_mean, ctrl_packets_gmtp_ci_upper, ctrl_packets_gmtp_ci_delta, total_repeticoes))

        file_tratamentos_denacast.close()
        file_tratamentos_ccntv.close()
        file_tratamentos_gmtp.close()
        file_tabela_latex_gmtp_denacast.close()
        file_tabela_latex_gmtp_ccntv.close()

        file_tratamentos_denacast = open("results/denacast-tratamento.txt", 'r+')
        calculate_error(file_tratamentos_denacast)
        file_tratamentos_denacast = open("results/denacast-tratamento.txt", 'r+')
        check_confidence_interval(file_tratamentos_denacast)
        file_tratamentos_ccntv = open("results/ccntv-tratamento.txt", 'r+')
        calculate_error(file_tratamentos_ccntv)
        file_tratamentos_ccntv = open("results/ccntv-tratamento.txt", 'r+')
        check_confidence_interval(file_tratamentos_ccntv)
        file_tratamentos_gmtp = open("results/gmtp-tratamento.txt", 'r+')
        calculate_error(file_tratamentos_gmtp)
        file_tratamentos_gmtp = open("results/gmtp-tratamento.txt", 'r+')
        check_confidence_interval(file_tratamentos_gmtp)

        file_tratamentos_denacast.close()
        file_tratamentos_ccntv.close()
        file_tratamentos_gmtp.close()


    # carrega tratamentos finais e gera tabelas do latex
    # ./parse-means.py 1 3
    if subopt == "3":
        fd1 = open("results/denacast-tratamento.txt", 'r')
        file_tratamentos_denacast = fd1.readlines()
        fd1.close()

        fd2 = open("results/ccntv-tratamento.txt", 'r')
        file_tratamentos_ccntv = fd2.readlines()
        fd2.close()

        fd3 = open("results/gmtp-tratamento.txt", 'r')
        file_tratamentos_gmtp = fd3.readlines()
        fd3.close()

        file_tabela_latex_gmtp_denacast = open("../../documento/analise-desempenho-mudccp-denacast.tex", 'w')
        file_tabela_latex_gmtp_ccntv = open("../../documento/analise-desempenho-mudccp-ccntv.tex", 'w')

        for tratamento_i in range(0, len(tratamentos_array)):
            tratamento_total_repeticoes = tratamentos_array[tratamento_i][0]

            line_tratamento_denacast = file_tratamentos_denacast[tratamento_i]
            line_tratamento_ccntv = file_tratamentos_ccntv[tratamento_i]
            line_tratamento_gmtp = file_tratamentos_gmtp[tratamento_i]

            line_tratamento_denacast_splitted = line_tratamento_denacast.split("\t")
            line_tratamento_ccntv_splitted = line_tratamento_ccntv.split("\t")
            line_tratamento_gmtp_splitted = line_tratamento_gmtp.split("\t")


            total_repeticoes_sqrt = numpy.sqrt(tratamento_total_repeticoes)

            # DENACAST
            startup_delay_denacast_mean = float(line_tratamento_denacast_splitted[1])
            startup_delay_denacast_ci_lower = float(line_tratamento_denacast_splitted[0])
            startup_delay_denacast_ci_upper = float(line_tratamento_denacast_splitted[2])
            startup_delay_denacast_ci = "(%.2f - %.2f)" % (startup_delay_denacast_ci_lower, startup_delay_denacast_ci_upper) 

            cont_index_denacast_mean = float(line_tratamento_denacast_splitted[5])
            cont_index_denacast_ci_lower = float(line_tratamento_denacast_splitted[4]) 
            cont_index_denacast_ci_upper = float(line_tratamento_denacast_splitted[6])
            cont_index_denacast_ci = "(%.2f - %.2f)" % (cont_index_denacast_ci_lower, cont_index_denacast_ci_upper) 

            distortion_denacast_mean = float(line_tratamento_denacast_splitted[9])
            distortion_denacast_ci_lower = float(line_tratamento_denacast_splitted[8])
            distortion_denacast_ci_upper = float(line_tratamento_denacast_splitted[10])
            distortion_denacast_ci = "(%.2f - %.2f)" % (distortion_denacast_ci_lower, distortion_denacast_ci_upper) 

            server_conn_denacast_mean = float(line_tratamento_denacast_splitted[13])
            server_conn_denacast_ci_lower = float(line_tratamento_denacast_splitted[12])
            server_conn_denacast_ci_upper = float(line_tratamento_denacast_splitted[14])
            server_conn_denacast_ci = "(%.2f - %.2f)" % (server_conn_denacast_ci_lower, server_conn_denacast_ci_upper) 

            client_conn_denacast_mean = float(line_tratamento_denacast_splitted[18])
            client_conn_denacast_ci_lower = float(line_tratamento_denacast_splitted[17])
            client_conn_denacast_ci_upper = float(line_tratamento_denacast_splitted[19])
            client_conn_denacast_ci = "(%.2f - %.2f)" % (client_conn_denacast_ci_lower, client_conn_denacast_ci_upper) 

            ctrl_packets_denacast_mean = float(line_tratamento_denacast_splitted[21])
            ctrl_packets_denacast_ci_lower = float(line_tratamento_denacast_splitted[20])
            ctrl_packets_denacast_ci_upper = float(line_tratamento_denacast_splitted[22])
            ctrl_packets_denacast_ci = "(%.2f - %.2f)" % (ctrl_packets_denacast_ci_lower, ctrl_packets_denacast_ci_upper) 

            # CCNTV
            startup_delay_ccntv_mean = float(line_tratamento_ccntv_splitted[1])
            startup_delay_ccntv_ci_lower = float(line_tratamento_ccntv_splitted[0])
            startup_delay_ccntv_ci_upper = float(line_tratamento_ccntv_splitted[2])
            startup_delay_ccntv_ci = "(%.2f - %.2f)" % (startup_delay_ccntv_ci_lower, startup_delay_ccntv_ci_upper) 

            cont_index_ccntv_mean = float(line_tratamento_ccntv_splitted[5])
            cont_index_ccntv_ci_lower = float(line_tratamento_ccntv_splitted[4]) 
            cont_index_ccntv_ci_upper = float(line_tratamento_ccntv_splitted[6])
            cont_index_ccntv_ci = "(%.2f - %.2f)" % (cont_index_ccntv_ci_lower, cont_index_ccntv_ci_upper) 

            distortion_ccntv_mean = float(line_tratamento_ccntv_splitted[9])
            distortion_ccntv_ci_lower = float(line_tratamento_ccntv_splitted[8])
            distortion_ccntv_ci_upper = float(line_tratamento_ccntv_splitted[10])
            distortion_ccntv_ci = "(%.2f - %.2f)" % (distortion_ccntv_ci_lower, distortion_ccntv_ci_upper) 

            server_conn_ccntv_mean = float(line_tratamento_ccntv_splitted[13])
            server_conn_ccntv_ci_lower = float(line_tratamento_ccntv_splitted[12])
            server_conn_ccntv_ci_upper = float(line_tratamento_ccntv_splitted[14])
            server_conn_ccntv_ci = "(%.2f - %.2f)" % (server_conn_ccntv_ci_lower, server_conn_ccntv_ci_upper) 

            client_conn_ccntv_mean = float(line_tratamento_ccntv_splitted[18])
            client_conn_ccntv_ci_lower = float(line_tratamento_ccntv_splitted[17])
            client_conn_ccntv_ci_upper = float(line_tratamento_ccntv_splitted[19])
            client_conn_ccntv_ci = "(%.2f - %.2f)" % (client_conn_ccntv_ci_lower, client_conn_ccntv_ci_upper) 

            ctrl_packets_ccntv_mean = float(line_tratamento_ccntv_splitted[21])
            ctrl_packets_ccntv_ci_lower = float(line_tratamento_ccntv_splitted[20])
            ctrl_packets_ccntv_ci_upper = float(line_tratamento_ccntv_splitted[22])
            ctrl_packets_ccntv_ci = "(%.2f - %.2f)" % (ctrl_packets_ccntv_ci_lower, ctrl_packets_ccntv_ci_upper) 

            # GMTP
            startup_delay_gmtp_mean = float(line_tratamento_gmtp_splitted[1])
            startup_delay_gmtp_ci_lower = float(line_tratamento_gmtp_splitted[0])
            startup_delay_gmtp_ci_upper = float(line_tratamento_gmtp_splitted[2])
            startup_delay_gmtp_ci = "(%.2f - %.2f)" % (startup_delay_gmtp_ci_lower, startup_delay_gmtp_ci_upper) 

            cont_index_gmtp_mean = float(line_tratamento_gmtp_splitted[5])
            cont_index_gmtp_ci_lower = float(line_tratamento_gmtp_splitted[4]) 
            cont_index_gmtp_ci_upper = float(line_tratamento_gmtp_splitted[6])
            cont_index_gmtp_ci = "(%.2f - %.2f)" % (cont_index_gmtp_ci_lower, cont_index_gmtp_ci_upper) 

            distortion_gmtp_mean = float(line_tratamento_gmtp_splitted[9])
            distortion_gmtp_ci_lower = float(line_tratamento_gmtp_splitted[8])
            distortion_gmtp_ci_upper = float(line_tratamento_gmtp_splitted[10])
            distortion_gmtp_ci = "(%.2f - %.2f)" % (distortion_gmtp_ci_lower, distortion_gmtp_ci_upper) 

            server_conn_gmtp_mean = float(line_tratamento_gmtp_splitted[13])
            server_conn_gmtp_ci_lower = float(line_tratamento_gmtp_splitted[12])
            server_conn_gmtp_ci_upper = float(line_tratamento_gmtp_splitted[14])
            server_conn_gmtp_ci = "(%.2f - %.2f)" % (server_conn_gmtp_ci_lower, server_conn_gmtp_ci_upper) 

            client_conn_gmtp_mean = float(line_tratamento_gmtp_splitted[18])
            client_conn_gmtp_ci_lower = float(line_tratamento_gmtp_splitted[17])
            client_conn_gmtp_ci_upper = float(line_tratamento_gmtp_splitted[19])
            client_conn_gmtp_ci = "(%.2f - %.2f)" % (client_conn_gmtp_ci_lower, client_conn_gmtp_ci_upper) 

            ctrl_packets_gmtp_mean = float(line_tratamento_gmtp_splitted[21])
            ctrl_packets_gmtp_ci_lower = float(line_tratamento_gmtp_splitted[20])
            ctrl_packets_gmtp_ci_upper = float(line_tratamento_gmtp_splitted[22])
            ctrl_packets_gmtp_ci = "(%.2f - %.2f)" % (ctrl_packets_gmtp_ci_lower, ctrl_packets_gmtp_ci_upper) 


            file_tabela_latex_gmtp_denacast.write("\multirow{2}{*}{\\textcolor{black}{\\bfseries GMTP}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ \\\\ \multirow{2}{*}{\\textcolor{black}{\\bfseries %d}} & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\\n \cline{2-7} \n \multirow{2}{*} {\\textcolor{black}{\\bfseries Denacast}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ \\\\\n & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\ \hline\n" % (startup_delay_gmtp_mean, cont_index_gmtp_mean, distortion_gmtp_mean, server_conn_gmtp_mean, client_conn_gmtp_mean, ctrl_packets_gmtp_mean, tratamento_i+1, startup_delay_gmtp_ci, cont_index_gmtp_ci, distortion_gmtp_ci,  server_conn_gmtp_ci, client_conn_gmtp_ci, ctrl_packets_gmtp_ci, startup_delay_denacast_mean, cont_index_denacast_mean, distortion_denacast_mean, server_conn_denacast_mean, client_conn_denacast_mean, ctrl_packets_denacast_mean, startup_delay_denacast_ci, cont_index_denacast_ci, distortion_denacast_ci, server_conn_denacast_ci, client_conn_denacast_ci, ctrl_packets_denacast_ci))
            file_tabela_latex_gmtp_ccntv.write("\multirow{2}{*}{\\textcolor{black}{\\bfseries GMTP}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ \\\\ \multirow{2}{*}{\\textcolor{black}{\\bfseries %d}} & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\\n \cline{2-7} \n \multirow{2}{*} {\\textcolor{black}{\\bfseries CCN-TV}} & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ & $%.2f$ \\\\\n & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ & $%s$ \\\\ \hline\n" % (startup_delay_gmtp_mean, cont_index_gmtp_mean, distortion_gmtp_mean, server_conn_gmtp_mean, client_conn_gmtp_mean, ctrl_packets_gmtp_mean, tratamento_i+1, startup_delay_gmtp_ci, cont_index_gmtp_ci, distortion_gmtp_ci,  server_conn_gmtp_ci, client_conn_gmtp_ci, ctrl_packets_gmtp_ci, startup_delay_ccntv_mean, cont_index_ccntv_mean, distortion_ccntv_mean, server_conn_ccntv_mean, client_conn_ccntv_mean, ctrl_packets_ccntv_mean, startup_delay_ccntv_ci, cont_index_ccntv_ci, distortion_ccntv_ci,  server_conn_ccntv_ci, client_conn_ccntv_ci, ctrl_packets_ccntv_ci))


        file_tabela_latex_gmtp_denacast.close()
        file_tabela_latex_gmtp_ccntv.close()

else:
    print("Opcao invalida.")

