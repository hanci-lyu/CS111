lab0: lab0.c
	@gcc -Wall -g -o $@ lab0.c
check: lab0
	@echo "this is a test" > input.txt
	@./lab0 --input=input.txt --output=output.txt || (echo 'Test fail: basic function fails'; exit 1)
	@diff input.txt output.txt || (echo 'Test fail: input and output mismatch'; exit 1)
	@(./lab0 --input=notafile.txt 2> /dev/null; test $$? -eq 1)||(echo 'Test fail: expected exit code 1 when input file cannot be opened.'; exit 1)
	@rm -rf notopen.txt; touch notopen.txt; chmod 444 notopen.txt
	@(./lab0 --output=notopen.txt 2> /dev/null; test $$? -eq 2)||(echo 'Test fail: expected exit code 2 when output file cannot be opened.'; exit 1)
	@./lab0 --segfault --catch 2> /dev/null; test $$? -eq 3 || (echo 'Test fail: expected exit code 3 when a segmentation fault is caught'; exit 1)
	@rm -rf input.txt output.txt notopen.txt	
	@echo 'Passes all the tests!'
clean:
	@rm -rf lab0
dist:
	@tar -cf lab0-004617230.tar.gz segfault.png breakpoint.png lab0.c Makefile README
	
