CFLAGS += -std=c99 -g -lpthread
LDFLAGS += -pthread

test_miner: test_miner.o miner.o

game: miner.o game.o

brutesolver: miner.o brutesolver.o
