#include <scheduler.hh>

#include <iostream>

int main()
{
	using namespace scheduler;
	using namespace std::chrono_literals;

	Scheduler scheduler;
	std::stop_source stop;

	scheduler::Job_Id jobs[2];

	jobs[0] = scheduler.schedule(2s, [] { std::cout << "This job should be called after 2 seconds" << std::endl; });
	jobs[1] = scheduler.schedule(1s, [] { std::cout << "This job should be called after 1 second" << std::endl; });

	scheduler.schedule(jobs, [&] {
		std::cout << "This should be almost the last one" << std::endl;
		scheduler.schedule(1s, [&] {
			std::cout << "After 1 second, this is the last one" << std::endl;
			stop.request_stop();
		});
	});

	scheduler.activate(stop.get_token());
}
