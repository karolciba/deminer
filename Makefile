CFLAGS=-std=c99 -g -lpthread -O3
LDFLAGS=-pthread

test_miner: test_miner.o miner.o

game: miner.o game.o

brutesolver: miner.o brutesolver.o
