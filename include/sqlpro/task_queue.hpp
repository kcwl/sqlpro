#pragma once
#include <mutex>
#include <atomic>
#include <queue>
#include <condition_variable>

namespace sqlpro
{
	template<typename T>
	class task_queue
	{
	public:
		task_queue()
			: shut_down_(false)
		{

		}

	public:
		void push(T&& t)
		{
			std::lock_guard lg(mutex_);

			queue_.push(std::move(t));

			cv_.notify_one();
		}

		bool empty()
		{
			std::lock_guard lk(mutex_);

			return queue_.empty();
		}

		void pop(T& t)
		{
			std::unique_lock lk(mutex_);

			cv_.wait(lk, [&]()
					 {
						 return !queue_.empty() || shut_down_;
					 });

			if (queue_.empty() || shut_down_)
				return;

			t = queue_.front();

			queue_.pop();
		}

		void cancel()
		{
			std::lock_guard lk(mutex_);

			while (!queue_.empty())
			{
				T& value = queue_.front();

				release(value);

				queue_.pop();
			}

			shut_down_.store(true);

			cv_.notify_all();
		}

	private:
		template<typename R = T>
			requires(!std::is_pointer_v<R>)
		void release(const R&) {}

		template<typename R = T>
			requires(std::is_pointer_v<R>)
		void release(R& r)
		{
			delete r;
		}

	private:
		std::mutex mutex_;

		std::queue<T> queue_;

		std::condition_variable cv_;

		std::atomic_bool shut_down_;
	};
}