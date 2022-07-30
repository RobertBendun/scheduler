#include <scheduler.hh>

#include <iostream>

int main()
{
	using namespace scheduler;
	using namespace std::chrono_literals;

	Scheduler scheduler;
	std::stop_source stop;

	scheduler.schedule(3s, [&] { std::cout << "Last one, should close the app" << std::endl; stop.request_stop(); });
	scheduler.schedule(2s, [] { std::cout << "This job should be called after 2 seconds" << std::endl; });
	scheduler.schedule(1s, [] { std::cout << "This job should be called after 1 second" << std::endl; });

	scheduler.activate(stop.get_token());
}
