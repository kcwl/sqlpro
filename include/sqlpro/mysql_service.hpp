#pragma once
#include <mysql.h>
#include <string>
#include <vector>
#include <sqlpro/algorithm.hpp>
#include <sqlpro/error_code.hpp>
#include <sqlpro/sql_transaction.hpp>

namespace sqlpro
{
	class mysql_connect
	{
	public:
		template<typename... Args>
		explicit mysql_connect(Args&&... args)
			: mysql_ptr_(nullptr)
			, auto_reconnect_(true)
			, lock_(false)
		{
			static_assert(sizeof...(Args) == 5, "param error!");

			conn_info_ = std::make_tuple(std::forward<Args>(args)...);

			run(std::forward<Args>(args)...);
		}

		virtual ~mysql_connect()
		{
			shutdown();
		}

	public:
		template<typename... Args>
		bool run(Args&&... args)
		{
			shutdown();

			mysql_ptr_ = mysql_init(nullptr);

			if (mysql_ptr_ == nullptr)
			{
				return false;
			}

			auto unix_socket = local_socket();

			set_default_option();

			auto tp = std::make_tuple(mysql_ptr_, std::forward<Args>(args)..., unix_socket.c_str(), 0ul);

			if (std::apply(&mysql_real_connect, tp) == nullptr)
			{
				return false;
			}

			return true;
		}

		inline void shutdown()
		{
			if (mysql_ptr_ == nullptr)
				return;

			mysql_close(mysql_ptr_);

			mysql_ptr_ = nullptr;
		}

		void set_charset(const std::string& charset = "utf8")
		{
			charset_ = charset;
		}

		bool execute(const std::string& sql, error_code& ec)
		{
			if (!mysql_ptr_)
				return false;

			auto res = mysql_query(mysql_ptr_, detail::to_uft8(sql).c_str());

			if (res != 0)
			{
				ec = error_code(mysql_error(mysql_ptr_), mysql_errno(mysql_ptr_));

				return false;
			}

			return true;
		}

		template<typename T>
		T query(const std::string& sql, error_code& ec)
		{
			T results{};

			if (mysql_ptr_ == nullptr)
				return results;

			if (!execute(sql, ec))
				return results;

			auto res = mysql_store_result(mysql_ptr_);

			if (res == nullptr)
				return results;

			if constexpr (is_container_v<T>)
			{
				while (auto column = mysql_fetch_row(res))
				{
					results.push_back(sqlpro::detail::template to_struct<T>(column));
				}
			}
			else
			{
				static_assert(std::is_trivial_v<T> && std::is_standard_layout_v<T>, "T error!");

				auto column = mysql_fetch_row(res);
				results = detail::template to_struct<T>(column);
			}

			return results;
		}

		void begin_transaction()
		{
			error_code ec;

			execute("START TRANSACTION", ec);
		}

		void rollback_transaction()
		{
			error_code ec;

			execute("ROLLBACK", ec);
		}

		void commit_transaction()
		{
			error_code ec;

			execute("COMMIT", ec);
		}

		bool execute_transaction(std::shared_ptr<sql_transaction> trans)
		{
			bool result = true;

			if (trans->queries_.empty())
				return !result;

			begin_transaction();

			for (auto& iter : trans->queries_)
			{
				error_code ec;

				if (!execute(iter, ec))
				{
					rollback_transaction();
					result = false;
					break;
				}
			}

			commit_transaction();

			return result;
		}

		bool try_lock()
		{
			return !lock_;
		}

		void lock()
		{
			lock_.store(true);
		}

		void unlock()
		{
			if (lock_.load() == true)
				lock_.store(false);
		}

	private:
		void set_default_option()
		{
			mysql_options(mysql_ptr_, MYSQL_OPT_RECONNECT, &auto_reconnect_);

			mysql_options(mysql_ptr_, MYSQL_SET_CHARSET_NAME, charset_.data());

			mysql_autocommit(mysql_ptr_, true);

			mysql_options(mysql_ptr_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
		}

		std::string local_socket()
		{
			std::string unix_socket{};

			auto host = std::get<0>(conn_info_);

			if (host == nullptr)
				return {};

			if (std::string(host).compare(".") == 0)
			{
#ifdef _WIN32
				unsigned int opt = MYSQL_PROTOCOL_PIPE;
#else
				unsigned int opt = MYSQL_PROTOCOL_SOCKET;

				unix_socket = std::get<5>(conn_info_);
#endif
				mysql_options(mysql_ptr_, MYSQL_OPT_PROTOCOL, (char*)&opt);
			}

			return unix_socket;
		}

	private:
		MYSQL* mysql_ptr_;

		bool auto_reconnect_;

		std::string charset_;

		std::tuple<const char*, const char*, const char*, const char*, unsigned int> conn_info_;

		std::atomic_bool lock_;
	};
}