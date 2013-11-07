#Por Isaac Maia, 11/07/2007

#Limpa Tudo
clean:
	rm -rf *~* *.log *.lot *.lol *.lof *.toc *.dvi *.aux  *.backup *.blg *.pdf


#Eh bom fazer duas compilacoes para nao dar problemas de referencias nao resolvidas
latex :
	latex -interaction=nonstopmode 'main.tex'
	bibtex 'main'
	latex -interaction=nonstopmode 'main.tex'
	bibtex 'main'

#cria PDF
pdf:	latex
	dvipdfm -o tese-leandro-melo-sales.pdf main.dvi
