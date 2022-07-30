#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <span>
#include <optional>

namespace scheduler
{
	using Job_Id = int;
	using Clock = std::chrono::steady_clock;

	struct Job
	{
		Clock::duration offset;
		std::function<void()> work;

		auto operator<=>(Job const& rhs) const -> std::strong_ordering
		{
			return offset <=> rhs.offset;
		}
	};

	struct Scheduler
	{
		// Schedule job based on time that it should be run
		Job_Id schedule(std::chrono::steady_clock::duration when, std::function<void()> job);

		// Schedule job after given set of jobs
		Job_Id schedule(std::span<Job_Id> jobs_before, std::function<void()> job);

		// Run scheduler in given thread
		void activate(std::stop_token);

		// Add given thread to possible threads of execution
		void add_execution_thread();

		std::priority_queue<Job, std::vector<Job>, std::greater<Job>> jobs;
		std::condition_variable_any waiting_flag;
		std::mutex queue_mutex;
		Clock::time_point start_time;
	};
}

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
