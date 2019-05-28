#include <QtTools/gui_executor.hqt>

namespace QtTools
{
	void gui_executor::delayed_task_continuation::continuate(shared_state_basic * caller) noexcept
	{
		auto owner = m_owner;
		bool notify, should_emit;

		if (not mark_marked())
			// gui_executor is destructed or destructing
			return;

		// remove ourself from m_delayed and add m_task to gui_executor tasks list
		{
			std::lock_guard lk(owner->m_mutex);

			auto & list = owner->m_delayed;
			auto & delayed_count = owner->m_delayed_count;
			auto it = list.iterator_to(*this);
			list.erase(it);

			owner->m_tasks.push_back(*m_task.release());
			notify = delayed_count != 0 and --delayed_count == 0;
			should_emit = std::exchange(owner->m_should_emit, false);
		}

		// notify gui_executor if needed
		if (notify) owner->m_event.notify_one();
		// send signal into gui thread if needed.
		else if (should_emit) owner->emit_actions_availiable();

		// we were removed from m_delayed - intrusive list,
		// which does not manage lifetime, decrement refcount
		release();
	}

	void gui_executor::delayed_task_continuation::abandone() noexcept
	{
		m_task->task_abandone();
		m_task = nullptr;
	}

	auto gui_executor::take_actions() -> task_list_type
	{
		decltype(m_tasks) tasks;
		{
			std::lock_guard lk(m_mutex);
			tasks = std::move(m_tasks);
			m_should_emit = true;
		}

		return tasks;
	}

	void gui_executor::emit_accamulated_actions()
	{
		auto tasks = take_actions();
		while (not tasks.empty())
		{
			auto * task = &tasks.front();
			task->task_execute();
			tasks.pop_front();
			task->task_release();
		}
	}

	gui_executor::gui_executor(QObject * parent)
		: QObject(parent)
	{
		connect(this, &gui_executor::emit_actions_availiable,
		        this, &gui_executor::emit_accamulated_actions,
		        Qt::QueuedConnection);
	}

	void gui_executor::clear() noexcept
	{
		// clear and abandon any tasks, including delayed ones
		task_list_type tasks;

		{
			// cancel/take delayed tasks, see gui_executor body description
			std::unique_lock lk(m_mutex);
			assert(m_delayed_count == 0);
			for (auto it = m_delayed.begin(); it != m_delayed.end();)
			{
				if (not it->mark_marked())
					++m_delayed_count, ++it;
				else
				{
					auto & item = *it;
					it = m_delayed.erase(it);
					item.abandone();
					item.release();
				}
			}

			// wait until all delayed_tasks are finished, and take pending tasks
			m_event.wait(lk, [this] { return m_delayed_count == 0; });
			tasks.swap(m_tasks);
		}

		tasks.clear_and_dispose([](task_base * task)
		{
			task->task_abandone();
			task->task_release();
		});
	}

	gui_executor::~gui_executor() noexcept
	{
		// If this object is destroyed nobody should be invoking any methods of this class
		// (with exception our internal classes, like delayed_task_continuation).
		clear();
	}
}
