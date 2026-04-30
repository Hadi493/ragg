# all: abstract-gg

# abstract-gg: main.c
# 	gcc -Wall -Wextra -o abstract-gg main.c -lm -O3 && ./abstract-gg output.ppm && magick output.ppm output.jpg

# gen: abstract-gg
# 	 ./abstract-gg output.ppm && magick output.ppm output.jpg

v1:
	 ./versions/agg-v1 output.ppm && magick output.ppm output.jpg

v2:
	 ./versions/agg-v2 output.ppm && magick output.ppm output.jpg

v3:
	 ./versions/agg-v3 output.ppm && magick output.ppm output.jpg


clean:
	rm -f *.ppm
