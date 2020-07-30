ascia: $(FILES_OBJ) $(HEADERS)
	@echo "- compiling target..."
	gcc -o ascia src/ascia.c -lcrypto
	@echo "> COMPLETED: ascia compiled."

.PHONY: clean
clean:
	@rm -f ascia
	@echo "> ascia cleaned"