#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <span>
#include <unordered_map>
#include <unordered_set>

namespace scheduler
{
	using Job_Id = unsigned long long;
	using Clock = std::chrono::steady_clock;
	struct Scheduler;
	struct Job;

	struct Job
	{
		Job_Id id;
		Clock::duration offset;
		std::function<void()> work;

		struct Compare
		{
			Scheduler const* s;
			bool operator()(Job const& lhs, Job const& rhs) const;
		};
	};

	using Priority_Queue_Base = std::priority_queue<Job, std::vector<Job>, Job::Compare>;

	struct Priority_Queue : Priority_Queue_Base
	{
		inline void change_scheduler(Scheduler const* s) { comp.s = s; }

		auto begin() { return c.begin(); }
		auto end() { return c.end(); }
	};

	struct Scheduler
	{
		Scheduler();
		Scheduler(Scheduler const&) = delete;
		Scheduler(Scheduler &&) = delete;

		Scheduler& operator=(Scheduler const&) = delete;
		Scheduler& operator=(Scheduler &&) = delete;

		// Schedule job based on time that it should be run
		Job_Id schedule(std::chrono::steady_clock::duration when, std::function<void()> job);

		// Schedule job after given set of jobs
		Job_Id schedule(std::span<Job_Id> jobs_before, std::function<void()> job);

		// Run scheduler in given thread
		void activate(std::stop_token);

		// Add given thread to possible threads of execution
		void add_execution_thread();

		// Allocates new job id (for internal use)
		Job_Id acquire_new_job_id();

		Priority_Queue jobs{};
		std::condition_variable_any waiting_flag{};
		std::mutex queue_mutex{};
		Clock::time_point start_time{};
		Job_Id next_job_id = 0;
		std::unordered_map<Job_Id, unsigned> dependencies_count;
		std::unordered_multimap</* predecessor */ Job_Id, /* successor */ Job_Id> successors;
		std::unordered_set<Job_Id> pending;
	};
}

