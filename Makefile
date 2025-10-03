# Makefile na raiz
CC      = mpicc
CFLAGS  = -O2 -Wall -Wextra -std=c11
LDFLAGS =

SRC_DIR = src
BUILD   = build
OUTDIR  = out

all: prepdirs $(BUILD)/mpi_wordcount $(BUILD)/mpi_artistcount

prepdirs:
	mkdir -p $(BUILD) $(OUTDIR)

$(BUILD)/mpi_wordcount: $(SRC_DIR)/mpi_wordcount.c $(SRC_DIR)/common_hash.h
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/mpi_wordcount.c $(LDFLAGS)

$(BUILD)/mpi_artistcount: $(SRC_DIR)/mpi_artistcount.c $(SRC_DIR)/common_hash.h
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/mpi_artistcount.c $(LDFLAGS)

clean:
	rm -rf $(BUILD) $(OUTDIR)
