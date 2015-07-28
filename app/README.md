GMTP Application Examples
=========================================

## Introduction ##

A guide to help the use of sample applications to test some specific features of GMTP protocol.


## GMTP example apps ##

For the environment used in the tests, you need to install python

Navigating to the directory of python apps:


    $ cd ~/gmtp/app/python
    
The applications are listed below:

- server.py

### Overview ###

Create a GMTP socket, defines an initial transmission rate to 50000 bytes/s, and begins to wait a client connection with ip and port previously defined.
when a client connects, starts the transmission of a message ("welcome do the jungle"). Information about transmission rate, size sent and time spent to send the message will be shown on the terminal where the server is running. The program is interrupted when any key is pressed. Logs will be uploaded to the log file in /var/log/syslog.
    
        Available options:
            -i, specifies the network interface.
            -a, IP address.
            -p, port that will be used to wait connection from client.
            example of use: 
            $ ./server.py -i eth0 -a 10.0.2.15 -p 12345
            
- client.py

### Overview ###

Create a GMTP socket and try to connect to server, with information specified as follows, and if the connection is successful, the message received are displayed, and information about transmission rate, size received and time spent to receive the message will be shown on the terminal where the client is running. Logs will be uploaded to the log file in 
/var/log/syslog.

        Available options:
            -i, specifies the network interface.
            -a, IP address.
            -p, port that will be used to connect to server.
            example of use: 
            $ ./client.py -i eth0 -a 10.0.2.15 -p 12345
            
    - server_multithreading.py
    - server_audio.py


## GMTP GStreamer plugin

	$ ~/gmtp/app/gstreamer/gst-plugin-gmtp
    
##  Gmtp Ns-3 ##

### Overview ###

Ns-3 [1] is a discrete-event network simulator, targeted primarily for research and educational use. ns-3 is free software, licensed under the GNU GPLv2 license, and is publicly available for research, development, and use.


Requirements
==========-

References:
===========
[1] https://www.nsnam.org/
