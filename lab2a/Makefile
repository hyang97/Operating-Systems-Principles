build: add list

add: lab2_add.c
	gcc -g -pthread lab2_add.c -o lab2_add

list: lab2_list.c SortedList.c SortedList.h
	gcc -g -pthread lab2_list.c SortedList.c -o lab2_list

tests:
	./tests.sh

graphs:
	/u/cs/grad/zhou/iloveos/gnuplot lab2_add.gp
	/u/cs/grad/zhou/iloveos/gnuplot lab2_list.gp

tarball:
	tar -cvzf lab2a-204584728.tar.gz lab2_add.c lab2_list.c SortedList.h SortedList.c lab2_add.csv lab2_list.csv Makefile README tests.sh *.png

clean:
	rm *.png
	rm *.csv
	rm lab2_add lab2_list
