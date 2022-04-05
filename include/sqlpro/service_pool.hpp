#pragma once
#include <memory>
#include <vector>
#include <thread>
#include <algorithm>
#include <future>
#include "task_queue.hpp"
#include "detail/service/mysql_connect.hpp"

namespace sqlpro
{
	class service_pool
	{
		using service_ptr = std::shared_ptr<mysql_connect>;

		inline constexpr static std::size_t pool_size = 2 * 3;

	public:
		//explicit service_pool(const std::string& host, const std::string& user, const std::string& passwd, const std::string& db, int port, std::size_t size = pool_size)
		template<typename... Args>
		explicit service_pool(std::size_t size, Args&&... args)
			: service_pos_(0)
			, stop_(false)
		{
			for (std::size_t i = 0; i < size; i++)
			{
				service_pool_.push_back(std::make_shared<mysql_connect>(std::forward<Args>(args)...));

				thread_pool_.push_back(std::make_shared<std::thread>([&]()
																	 {
																		 while (!stop_)
																		 {
																			 std::function<void()> task;

																			 cq_.pop(task);

																			 if (task == nullptr)
																				 continue;

																			 task();
																		 }
																	 }));
			}
		}

		~service_pool()
		{
			stop();
		}

		void run()
		{
			mysql_init(nullptr);
		}

		void stop()
		{
			std::for_each(service_pool_.begin(), service_pool_.end(), [](auto iter)
						  {
							  iter->shutdown();
						  });

			service_pool_.clear();

			cq_.cancel();

			stop_.store(true);

			std::for_each(thread_pool_.begin(), thread_pool_.end(), [](auto iter)
						  {
							  iter->join();
						  });
		}

		service_ptr get_service()
		{
			auto iter = std::find_if(service_pool_.begin(), service_pool_.end(),
									 [](auto srv)
									 {
										 return srv->try_lock();
									 });

			if (iter == service_pool_.end())
				return nullptr;

			return *iter;
		}

		template<typename F, typename... Args>
		auto async_execute(F&& f, Args&&... args)
		{
			return async_execute(std::format(std::forward<F>(f), std::forward<Args>(args)...));
		}

		auto async_execute(const std::string& sql)
		{
			auto conn_ptr = get_service();

			if (conn_ptr == nullptr)
				return std::future<bool>();

			auto task = std::make_shared<std::packaged_task<bool()>>(
				[&]
				{
					error_code ec;
					auto res = conn_ptr->execute(sql, ec);

					conn_ptr->unlock();

					return res;
				});

			push_queue(task);

			return task->get_future();
		}

		template<typename T, typename F, typename... Args>
		auto async_pquery(F&& f, Args&&... args)
		{
			return async_query<T>(std::format(std::forward<F>(f), std::forward<Args>(args)...));
		}

		template<typename T>
		auto async_query(const std::string& sql)
		{
			auto conn_ptr = get_service();

			if (conn_ptr == nullptr)
				return std::future<T>{};

			auto task = std::make_shared<std::packaged_task<T()>>(
				[&, conn_ptr]
				{
					error_code ec;

					auto result = conn_ptr->query<T>(sql, ec);

					conn_ptr->unlock();

					return result;
				});

			auto future = task->get_future();

			push_queue(task);

			return future;
		}

		auto async_transaction(std::shared_ptr<sql_transaction> trans_ptr)
		{
			auto conn_ptr = get_service();

			if (conn_ptr == nullptr)
				return std::future<bool>{};

			auto task = std::make_shared<std::packaged_task<bool()>>(
				[&]
				{
					error_code ec;

					auto result = conn_ptr->execute_transaction(trans_ptr);

					conn_ptr->unlock();

					return result;
				});

			push_queue(task);

			return task->get_future();
		}

		template<typename F>
		void push_queue(F&& task)
		{
			cq_.push([task = std::move(task)]{ (*task)(); });
		}


	private:
		std::vector<service_ptr> service_pool_;

		task_queue<std::function<void()>> cq_;

		std::size_t service_pos_;

		std::vector<std::shared_ptr<std::thread>> thread_pool_;

		std::atomic_bool stop_;
	};
}