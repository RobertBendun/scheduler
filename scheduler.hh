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
	/// Specified unique per job id for dependency referencing
	using Job_Id = unsigned long long;

	/// Clock used to run time based tasks
	using Clock = std::chrono::steady_clock;

	struct Scheduler;
	struct Job;

	/// Internal representation of job
	struct Job
	{
		/// Job id
		Job_Id id;

		/// Time when job must be runned (relative to Scheduler::start_time)
		Clock::duration offset;

		/// Work that job must done
		std::function<void()> work;

		/// Custom comparator for priority_queue. We don't want to define general ordering for this type.
		struct Compare
		{
			bool operator()(Job const& lhs, Job const& rhs) const;
		};
	};

	/// Specialization of priority_queue template with custom Job comparator.
	using Priority_Queue = std::priority_queue<Job, std::vector<Job>, Job::Compare>;

	struct Scheduler
	{
		Scheduler() = default;
		Scheduler(Scheduler const&) = delete;
		Scheduler(Scheduler &&) = delete;

		Scheduler& operator=(Scheduler const&) = delete;
		Scheduler& operator=(Scheduler &&) = delete;

		/// Schedule job based on time that it should be run
		Job_Id schedule(Clock::duration when, std::function<void()> job);

		/// Schedule job after given set of jobs. If completed or never created Job_Id is passed
		/// then it is ignored. Only valid predecessors are those in the system at the moment
		/// of the function call
		Job_Id schedule(std::span<Job_Id> jobs_before, std::function<void()> job);

		/// Run scheduler in given thread
		void activate(std::stop_token);

		/// Add given thread to possible threads of execution (currently unsupported)
		void add_execution_thread();

		/// Allocates new job id (for internal use)
		Job_Id acquire_new_job_id();

		/// Collection of all the jobs that are ready to execute (without depedencies)
		Priority_Queue ready_jobs{};

		// Collection of all the jobs that are waiting for their dependencies to resolve
		std::unordered_map<Job_Id, Job> jobs_with_dependencies{};

		/// Flag that notifies updates into `jobs`
		std::condition_variable_any waiting_flag{};

		/// Mutex protecting modification of any data in this struct
		std::mutex queue_mutex{};

		/// Global time from which durations are offseted
		Clock::time_point start_time{};

		/// Next available identifier for a job
		Job_Id next_job_id = 0;

		/// Count of depedencies of jobs with depedencies. Depedency-free jobs are not in this map
		std::unordered_map<Job_Id, unsigned> dependencies_count;

		/// Map from predecessors to successors representing dependency graph
		std::unordered_multimap</* predecessor */ Job_Id, /* successor */ Job_Id> successors;

		/// List of all jobs id currently in the system
		std::unordered_set<Job_Id> pending;
	};
}

