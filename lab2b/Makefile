build: add list

list: lab2_list.c SortedList.c SortedList.h
	gcc -g -pthread lab2_list.c SortedList.c -o lab2_list

add: lab2_add.c
	gcc -g -pthread lab2_add.c -o lab2_add

tests:
	-./tests.sh

profile:
	-rm -f ./raw.gpref
	LD_PRELOAD=/u/cs/ugrad/zihengy/lib/libprofiler.so.0 CPUPROFILE=./raw.gpref ./lab2_list --threads=12 --iterations=1000 --sync=s
	pprof --text ./lab2_list ./raw.gpref > profile.gpref
	pprof --list=threadFunction ./lab2_list ./raw.gpref >> profile.gpref
	rm -f ./raw.gpref

graphs:
	/u/cs/grad/zhou/iloveos/gnuplot lab2b.gp

tarball:
	tar -cvzf lab2b-204584728.tar.gz SortedList.h SortedList.c lab2_add.c lab2_list.c Makefile lab_2b_list_1-2.csv lab_2b_list_3-5.csv profile.gpref lab2b_1.png lab2b_2.png lab2b_3.png lab2b_4.png lab2b_5.png README.txt

clean:
	rm -f *.png
	rm -f *.csv

