SRC := gmtp
PWD := $(shell pwd)

all :  pdf txt

txt : latex
	catdvi -e 0 $(SRC).dvi > $(SRC).txt
	ln -sf $(PWD)/$(SRC).txt ../

txt2 : latex
	dvi2tty -a $(SRC).dvi > $(SRC)-2.txt
	ln -sf $(PWD)/$(SRC)-2.txt ../
	
	
pdf : latex
	pdflatex $(SRC).tex
	ln -sf $(PWD)/$(SRC).pdf ../

latex :
	latex $(SRC).tex
#	makeindex -s $(SRC).ist -t $(SRC).glg -o $(SRC).gls $(SRC).glo
	bibtex $(SRC)
	latex $(SRC).tex
	latex $(SRC).tex

clean :
	rm -rf *.txt *.pdf *.aux *.bbl *.blg *.dvi *.log *.ps *.toc *~ *.lof *.glo *.gls *.ilg *.ist *.glg *.synctex.gz *.brf *.idx
	rm -rf ../$(SRC).pdf ../$(SRC).txt ../$(SRC)-2.txt
