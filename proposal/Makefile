all: blank.pdf

blank.pdf: blank.tex
	latexmk blank -pdf 
	bibtex blank.aux
	latexmk blank -pdf 
	latexmk blank -pdf 
clean:
	rm -f *.aux *.log *.pdf *.dvi *.fdb_latexmk *.fls *.out *.bbl *.blg *.synctex.gz *.brf
