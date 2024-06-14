CC = g++
CFLAGS = -I. -std=c++20 
DEPS = 
LIB = -pthread

TARGETS = sample1Level sampleMultiLevel sampleQueue sampleMultiLevelPrint

all: $(TARGETS)

%: %.cpp
	$(CC) -o $@ $^ $(CFLAGS) $(LIB)

clean:
	rm -f *~
	rm -f ./sample1Level
	rm -f ./sampleMultiLevel
	rm -f ./sampleQueue
	rm -f ./sampleMultiLevelPrint
