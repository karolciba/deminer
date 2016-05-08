CFLAGS=-std=c99 -ggdb

test_miner: test_miner.o miner.o

game: miner.o game.o
