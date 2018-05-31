PAPER = a4
MKHOWTO = python tools/mkhowto --$(PAPER)

default: html

all: ps html text dvi

dvi ps text: pyOpenSSL.tex
	$(MKHOWTO) --$@ $^

html: pyOpenSSL.tex
	$(MKHOWTO) --html --iconserver . $^
	-rm -rf html/
	mkdir html/
	mv *.html pyOpenSSL.css html/

clean:
	rm -rf html/ *.html pyOpenSSL.[0-9a-su-z]* pyOpenSSL.txt pyOpenSSL.tmp

.PHONY: default all html dvi ps text clean
