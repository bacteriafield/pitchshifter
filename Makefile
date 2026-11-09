INCLUDE=src/core/config.h

.PHONY: all build run clean

all: build

run:
	./build/bin/PitchShifter

build:
	chmod +x build.sh
	./build.sh

clean:
	rm -fr $(INCLUDE)
	rm -fr build