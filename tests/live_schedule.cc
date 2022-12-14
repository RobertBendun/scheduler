#include <scheduler.hh>

#include <iostream>

int main()
{
	using namespace scheduler;
	using namespace std::chrono_literals;

	Scheduler scheduler;
	std::stop_source stop;

	scheduler.schedule(3s, [&] { std::cout << "Line #3" << std::endl; stop.request_stop(); });
	scheduler.schedule(1s, [&] {
		std::cout << "Line #1" << std::endl;
		scheduler.schedule(1s, [] { std::cout << "Line #2" << std::endl; });
	});

	scheduler.activate(stop.get_token());
}
