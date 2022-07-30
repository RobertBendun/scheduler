scheduler: scheduler.cc
	g++ -std=c++20 -Wall -Wextra -O2 $< -o $@

.PHONY: run
run: scheduler
	./scheduler
