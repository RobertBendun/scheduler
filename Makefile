.PHONY: all test
all: libscheduler.a

test:
	tests/time_based_schedule.out tests/schedule_with_dependencies.out tests/live_schedule.out

scheduler.o: scheduler.cc scheduler.hh
	g++ -std=c++20 -Wall -Wextra -O2 $< -o $@ -c

libscheduler.a: scheduler.o
	ar rcs $@ $<

tests/%.out: tests/%.cc scheduler.cc scheduler.hh
	g++ -std=c++20 -Wall -Wextra -O2 $< scheduler.cc -o $@ -I.

