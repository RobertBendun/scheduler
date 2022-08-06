#include "scheduler.hh"

#include <concepts>
#include <iterator>

using namespace scheduler;

// Support for range loops on iterator pairs for nicer code below
namespace std
{
	template<std::forward_iterator T1, std::sentinel_for<T1> T2>
	T1 begin(std::pair<T1, T2> const& range) { return range.first; }

	template<std::forward_iterator T1, std::sentinel_for<T1> T2>
	T2 end(std::pair<T1, T2> const& range) { return range.second; }
}

/// Activate runs scheduler until stop is issued.
///
/// While stop was not requested:
///   Wait at arrival of any dependency free jobs
///   If there is job:
///     If this job can be completed. (we passed time that was specified as time offset):
///				Remove it from jobs queue
///				Remove it from pending set
///				Drop dependencies_count of all successors of this job,
///					and insert them into ready jobs if dependencies_count drops to 0
///       And execute it (run work callback)
///     Otherwise:
///       Wait until job is ready or there are newer job
void Scheduler::activate(std::stop_token stop)
{
	while (!stop.stop_requested()) {
		std::unique_lock lock(queue_mutex);
		waiting_flag.wait(lock, stop, [this] { return !ready_jobs.empty(); });
		if (stop.stop_requested()) {
			return;
		}

		if (auto const start = start_time + ready_jobs.top().offset; start <= Clock::now()) {
			auto job = std::move(ready_jobs.top());
			ready_jobs.pop();
			pending.erase(job.id);

			// Since we will do job now, we can remove it from all completed jobs since postcondition of this
			// branch is job will be completed.
			for (auto [pred, succ] : successors.equal_range(job.id)) {
				// If dependencies_count of successor job has now 0 dependencies it can be executed,
				// so we can add it to ready_jobs priority queue. Additionaly we can free space in
				// dependencies count map since it no longer have any dependencies
				if (--dependencies_count[succ] == 0) {
					dependencies_count.erase(succ);
					if (auto successor = jobs_with_dependencies.find(succ); successor != jobs_with_dependencies.end()) {
						ready_jobs.push(std::move(successor->second));
						jobs_with_dependencies.erase(successor);
					}
				}
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

	if (ready_jobs.empty() && jobs_with_dependencies.empty()) {
		start_time = Clock::now();
	} else {
		when += Clock::now() - start_time;
	}

	auto const id = acquire_new_job_id();

	ready_jobs.push(scheduler::Job { .id = id, .offset = when, .work = std::move(job) });
	pending.insert(id);
	waiting_flag.notify_all();
	return id;
}

Job_Id Scheduler::schedule(std::span<Job_Id> jobs_before, std::function<void()> job)
{
	std::unique_lock lock(queue_mutex);

	if (ready_jobs.empty() && jobs_with_dependencies.empty()) {
		start_time = Clock::now();
	}

	auto id = acquire_new_job_id();

	// We add all dependencies from jobs_before if they are in the system.
	// We don't add those that aren't since they can be completed by now,
	// and keeping completed jobs history is wasteful.
	//
	// Possibly job can have all its dependencies resolved by now, so in that case
	// we can add it directly into ready_jobs
	bool has_pending_dependencies = false;
	for (auto const& predecessor : jobs_before) {
		if (pending.contains(predecessor)) {
			dependencies_count[id]++;
			successors.insert({ predecessor, id });
			has_pending_dependencies = true;
		}
	}

	if (has_pending_dependencies) {
		jobs_with_dependencies.insert({ id, scheduler::Job { .id = id, .offset = {}, .work = std::move(job) }});
	} else {
		ready_jobs.push(scheduler::Job { .id = id, .offset = {}, .work = std::move(job) });
	}

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
	return lhs.offset > rhs.offset;
}
