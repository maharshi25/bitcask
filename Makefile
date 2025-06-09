# Copyright 2024 Maharshi Shukla

# This file is part of bitcask.

# bitcask is free software: you can redistribute it and/or modify it under the terms of the GNU
# General Public License as published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.

# bitcask is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
# the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
# Public License for more details.

# You should have received a copy of the GNU General Public License along with bitcask. If not, see
# <https://www.gnu.org/licenses/>.

.POSIX:
.SUFFIXES:
CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -Wno-sign-compare -O3

all: bitcask
bitcask: src/alloc.o src/crcspeed.o src/crc64speed.o src/bitcask.o src/ht.o src/s8.o
	$(CC) $(LDFLAGS) -o bitcask alloc.o crcspeed.o crc64speed.o bitcask.o ht.o s8.o
src/alloc.o: src/alloc.c src/alloc.h
src/crcspeed.o: src/crcspeed.c src/crcspeed.h
src/crc64speed.o: src/crc64speed.c
src/bitcask.o: src/bitcask.c src/bitcask.h
src/ht.o: src/ht.c src/ht.h
src/s8.o: src/s8.c src/s8.h

clean:
	rm -f bitcask alloc.o crcspeed.o crc64speed.o bitcask.o ht.o s8.o

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -c $<
