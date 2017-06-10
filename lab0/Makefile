lab0: lab0.c
	gcc lab0.c -o lab0

smoketest: lab0
	@echo Hello World! > input.txt
	@./lab0 < input.txt > output.txt &&\
	diff input.txt output.txt > /dev/null &&\
	./lab0 --input=input.txt --output=output2.txt &&\
	diff input.txt output2.txt > /dev/null &&\
	echo "All tests succeeded"

clean:
	rm -f lab0 input.txt output.txt output2.txt

dist:
	tar cvzf lab0-204584728.tar.gz lab0.c Makefile README backtrace.png breakpoint.png
