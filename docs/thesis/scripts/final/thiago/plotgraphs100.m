#!/usr/bin/octave -qf

z = [3, 5, 7, 12, 17, 22, 25]
w = [0.2015833, 0.2083808, 0.2027205, 0.2073068, 0.2040853, 0.2025553, 0.1941442]
er = [0.0102702, 0.01618617, 0.01256699, 0.01576346, 0.01742051, 0.01241388, 0.0126949]

#gset xrange [0:30]

#axis([0 30 0 2], "autox"); 

plotColors = colormap;
#plotColorsI = round( size( plotColors, 1)* 1 / 3 );
hold on
h1 = errorbar(z,w,er);
hold off
#set( h1,'Color', plotColors( plotColorsI,:) );
set( h1,'Color', plotColors( 14,:) );

w1 = [0.2209996, 0.2454326, 0.2273837, 0.2405692, 0.2404947, 0.2323167, 0.2213233]
er2 = [0.01794994, 0.0261497, 0.02150092, 0.02651665, 0.02457504, 0.02108702, 0.01797994]

#plotColorsI = round( size( plotColors, 1)* 2 / 3 );
hold on
h2 = errorbar(z,w1,er2)
hold off
#set( h2,'Color', plotColors( plotColorsI,:) );
set( h2,'Color', plotColors( 30,:) );

w3 = [0.3082478, 0.3283795, 0.3127209, 0.3239744, 0.3207426, 0.3125613, 0.3008127]
er3 = [0.01913524, 0.02870482, 0.02320691, 0.02910637, 0.03019728, 0.02305522, 0.02079796]

#plotColorsI = round( size( plotColors, 1)* 3 / 3 );
hold on;
h3 = errorbar(z,w3,er3);
hold off;
set( h3,'Color', plotColors( 64,:) );
#set( h3,'Color', plotColors( plotColorsI,:) );
#set(h3,"color","red");

xlabel('Número de Veículos no Raio de Transmissão');
ylabel('Tempo de Detecção (s)');
title('2 Veículos Não-Sybil - Beacon 100 ms');

#disp ("The value of pi is:");
#legend([h1(1), h2(1), h3(1)], {"Solid", "Dashed", "thiago"});
#legend (h1, "sin (x)");
#legend (h2, "sin (u)");
#legend (h3, "sin (z)");

#w.legend = "w";
#w1.legend = "21";

legend ("4 Níveis", "5 Níveis", "6 Níveis", "location", "southeast");

s = get(0,'showhiddenhandles'); 
set(0,'showhiddenhandles','on'); 
newpos = [0.13 0.135 0.775 0.75];        %// default is [0.13 0.11 0.775 0.815] 
figs = get(0,'children'); 
if (~isempty(figs)) 
    for k=1:length(figs) 
        cax = get(figs(k),'currentaxes'); 
        pos = get(cax,'position');       
        if ~(pos(1) == newpos(1) && ... 
             pos(2) == newpos(2) && ... 
             pos(3) == newpos(3) && ... 
             pos(4) == newpos(4)) 
            set(cax,'position',newpos);     
            set(0,'currentfigure',figs(k)); 
            drawnow(); 
        endif 
    endfor 
endif 
set(0,'showhiddenhandles',s); 

