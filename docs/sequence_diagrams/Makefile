DOT_WSDs=$(shell ls *.wsd | sed -e "s/\.wsd/.png/")

all: ${DOT_WSDs}

%.png: %.wsd
	wsd --style earth $< $@
