paper.pdf: paper.tex
	pdflatex paper
	bibtex paper
	pdflatex paper
	pdflatex paper

clean:
	rm -f *.bbl *.blg *.log *.aux paper.pdf
