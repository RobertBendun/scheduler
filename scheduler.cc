#include "scheduler.hh"

#include <iterator>
#include <concepts>

using namespace scheduler;


// Support for range loops on iterator pairs for nicer code below
namespace std
{
	template<std::forward_iterator T1, std::sentinel_for<T1> T2>
	T1 begin(std::pair<T1, T2> const& range) { return range.first; }

	template<std::forward_iterator T1, std::sentinel_for<T1> T2>
	T2 end(std::pair<T1, T2> const& range) { return range.second; }
}

Scheduler::Scheduler()
{
	jobs.change_scheduler(this);
}

/// Activate runs scheduler until stop is issued.
///
/// While stop was not requested:
///   Wait at arrival of any dependency free jobs
///   If there is job:
///     If this job can be completed. (we passed time that was specified as time offset):
///				Remove it from jobs queue
///				Remove it from pending set
///				Drop dependencies_count of all successors of this job
///       And execute it (run work callback)
///     Otherwise:
///       Wait until job is ready or there are newer job
void Scheduler::activate(std::stop_token stop)
{
	while (!stop.stop_requested()) {
		std::unique_lock lock(queue_mutex);
		waiting_flag.wait(lock, stop, [this] { return !jobs.empty() && dependencies_count[jobs.top().id] == 0; });
		if (stop.stop_requested()) {
			return;
		}

		if (auto const start = start_time + jobs.top().offset; start <= Clock::now()) {
			auto job = std::move(jobs.top());
			jobs.pop();
			pending.erase(job.id);

			for (auto [pred, succ] : successors.equal_range(job.id)) {
				dependencies_count[succ]--;
			}

			lock.unlock();
			job.work();
		} else {
			waiting_flag.wait_until(lock, stop, start, std::false_type{});
		}
	}
}

Job_Id Scheduler::schedule(Clock::duration when, std::function<void()> job)
{
	std::unique_lock lock(queue_mutex);

	if (jobs.empty()) {
		start_time = Clock::now();
	} else {
		when += Clock::now() - start_time;
	}

	auto const id = acquire_new_job_id();

	jobs.push(scheduler::Job { .id = id, .offset = when, .work = std::move(job) });
	pending.insert(id);
	waiting_flag.notify_all();
	return id;
}

Job_Id Scheduler::schedule(std::span<Job_Id> jobs_before, std::function<void()> job)
{
	std::unique_lock lock(queue_mutex);

	if (jobs.empty()) {
		start_time = Clock::now();
	}

	auto id = acquire_new_job_id();

	for (auto const& predecessor : jobs_before) {
		if (pending.contains(predecessor)) {
			dependencies_count[id]++;
			successors.insert({ predecessor, id });
		}
	}

	jobs.push(scheduler::Job { .id = id, .offset = {}, .work = std::move(job) });
	pending.insert(id);
	waiting_flag.notify_all();
	return id;
}

Job_Id Scheduler::acquire_new_job_id()
{
	return next_job_id++;
}

auto find_or_default(auto const& container, auto const& key, auto&& def)
{
	if (auto it = container.find(key); it != std::end(container)) {
		return it->second;
	} else {
		return std::move(def);
	}
}

bool Job::Compare::operator()(Job const& lhs, Job const& rhs) const
{
	auto const lhs_count = find_or_default(s->dependencies_count, lhs.id, 0u);
	auto const rhs_count = find_or_default(s->dependencies_count, rhs.id, 0u);

	return lhs_count == rhs_count
		? lhs.offset > rhs.offset
		: lhs_count > rhs_count;
}
