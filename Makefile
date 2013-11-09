all:
	@echo "Please use Makefile.atmega8 or Makefile.atmega168."
	@echo
	@echo "Example: make -f Makefile.atmega168"

clean:
	make -f Makefile.atmega168 clean
	make -f Makefile.atmega8 clean
