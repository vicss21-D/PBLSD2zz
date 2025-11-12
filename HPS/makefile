# Makefile para compilação nativa no DE1-SoC

help:
	@echo "Comandos:"
	@echo "run: executa (compila tudo, executa e limpa)"
	@echo "clean: limpa arquivos compilados"

run:
	@echo "--- Montando lib.s ---"
	@as lib.s -o lib.o
	@echo "--- Compilando e Ligando (C) main.c ---"
	@gcc main.c lib.o -z noexecstack -std=c99 -lm -o exe
	@echo "--- Executando ---"
	@./exe
	@echo "--- Limpando arquivos temporários ---"
	@rm -f exe lib.o

clean:
	@echo "--- Limpando ---"
	rm -f exe *.o

.PHONY: help run clean
