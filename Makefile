all: abstract-gg

abstract-gg: main.c
	gcc -Wall -Wextra -o abstract-gg main.c -lm -O3 && ./abstract-gg output.ppm && magick output.ppm output.jpg

clean:
	rm -f *.ppm
