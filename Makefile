SHELL := /bin/bash

# ==================================================
# COMMANDS

CC = gcc
CFLAGS = -Wall -Wextra -g 
RM = rm -f
TARGET = tinyshell

# ==================================================
# TARGETS

all: $(TARGET) ## build the executable

# final link for executable
$(TARGET): tinyshell.o
	$(CC) $(CFLAGS) $^ -o $@

# generate objects
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# clean temporary files
clean:		## remove object files and backups
	$(RM) *.o *~

# remove executable
purge: clean ## remove object files, backups and the executable
	$(RM) $(TARGET)

# -------------------- Makefile help

help:                           ## Print help for Makefile list
	@grep -E '(^[a-zA-Z_-]+:.*?##.*$$)|(^# --------------------)' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}{printf "\033[32m %-35s\033[0m %s\n", $$1, $$2}' | sed -e 's/\[32m # --------------------/[33m===============/'