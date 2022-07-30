.PHONY: all
all: tests/time_based_schedule.out

tests/%.out: tests/%.cc scheduler.cc scheduler.hh
	g++ -std=c++20 -Wall -Wextra -O2 $< scheduler.cc -o $@ -I.

