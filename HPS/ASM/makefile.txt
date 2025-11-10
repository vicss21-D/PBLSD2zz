# Makefile para compilação nativa no DE1-SoC
# (Baseado no seu ficheiro)

# Alvo de ajuda
help:
	@echo "Comandos:"
	@echo "run: Roda código (compila tudo, executa e limpa)"
	@echo "clean: Remove os ficheiros compilados"

# Regra 'run': Compila tudo, executa e limpa
run:
	@echo "--- Montando (Assembly) lib.s ---"
	@as lib.s -o lib.o
	@echo "--- Compilando e Ligando (C) main.c ---"
	@gcc main.c lib.o -z noexecstack -std=c99 -lm -o exe
	@echo "--- Executando Aplicação ---"
	@./exe
	@echo "--- Limpando ficheiros temporários ---"
	@rm -f exe lib.o

# <<< REGRA 'CLEAN' ADICIONADA >>>
# Limpa todos os ficheiros compilados (objetos e executável)
clean:
	@echo "--- Limpando ficheiros compilados ---"
	rm -f exe *.o

.PHONY: help run clean
