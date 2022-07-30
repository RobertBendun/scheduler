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

