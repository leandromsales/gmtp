SRC += 01-gmtp

all :  pdf

txt : latex
	dvi2tty -a $(SRC).dvi > $(SRC).txt
	
pdf : latex
	pdflatex $(SRC).tex

latex :
	latex $(SRC).tex
#	makeindex -s $(SRC).ist -t $(SRC).glg -o $(SRC).gls $(SRC).glo
	bibtex $(SRC)
	latex $(SRC).tex
	latex $(SRC).tex

clean :
	rm -rf *.txt *.pdf *.aux *.bbl *.blg *.dvi *.log *.ps *.toc *~ *.lof *.glo *.gls *.ilg *.ist *.glg *.synctex.gz *.brf *.idx
