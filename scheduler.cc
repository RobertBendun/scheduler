#include "scheduler.hh"

void scheduler::Scheduler::activate(std::stop_token stop)
{
	while (!stop.stop_requested()) {
		std::unique_lock lock(queue_mutex);
		waiting_flag.wait(lock, stop, [this] { return !jobs.empty(); });
		if (stop.stop_requested()) {
			return;
		}

		if (auto const start = start_time + jobs.top().offset; start <= Clock::now()) {
			auto job = std::move(jobs.top());
			jobs.pop();
			lock.unlock();
			job.work();
		} else {
			waiting_flag.wait_until(lock, stop, start, std::false_type{});
		}
	}
}

scheduler::Job_Id scheduler::Scheduler::schedule(scheduler::Clock::duration when, std::function<void()> job)
{
	std::unique_lock lock(queue_mutex);

	if (jobs.empty()) {
		start_time = Clock::now();
	} else {
		when += Clock::now() - start_time;
	}

	jobs.push(scheduler::Job { .offset = when, .work = std::move(job) });
	waiting_flag.notify_all();
	return 0;
}
