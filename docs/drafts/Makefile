#
# Makefile for the GMTP drafts
#
# 12 Jun 2015, Wendell Soares <wss@ic.ufal.br>
#

DRAFTS += gmtp

all: pdf txt

pdf: 
	make -C $(DRAFTS) pdf
	
txt: 
	make -C $(DRAFTS) txt

clean: 
	make -C $(DRAFTS) clean
	rm -rf *.txt *.pdf *.aux *.bbl *.blg *.dvi *.log *.ps *.toc *~ *.lof *.glo *.gls *.ilg *.ist *.glg *.synctex.gz *.brf *.idx


