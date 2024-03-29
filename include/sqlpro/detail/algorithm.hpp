﻿#pragma once
#include <sstream>
#include <string>
#include <string_view>
#include <mysql.h>
#include <algorithm>
#include "type_traits.hpp"
#include "reflect/include/reflect.hpp"
#include <codecvt>

#pragma warning(disable:4996)
namespace sqlpro
{
	namespace detail
	{
		template<typename Tuple, typename Func, std::size_t... I>
		constexpr auto for_each(Tuple&& tuple, Func&& f, std::index_sequence<I...>)
		{
			return (std::forward<Func>(f)(reflect::get<I>(std::forward<Tuple>(tuple)), I), ...);
		}

		template<typename T, typename Func>
		constexpr auto for_each(T&& tp, Func&& f)
		{
			return detail::template for_each(std::forward<T>(tp), std::forward<Func>(f), std::make_index_sequence<reflect::rf_size_v<T>>{});
		}

		template<typename Tuple, typename Func, std::size_t... I>
		constexpr auto for_each_elem(Tuple&& tuple, Func&& f, std::index_sequence<I...>)
		{
			return (std::forward<Func>(f)(reflect::name<Tuple, I>(), reflect::get<I>(std::forward<Tuple>(tuple)), std::move(I)), ...);
		}

		template<typename T, typename Func>
		constexpr auto for_each_elem(T&& tp, Func&& f)
		{
			return detail::template for_each_elem(std::forward<T>(tp), std::forward<Func>(f), std::make_index_sequence<reflect::rf_size_v<T>>{});
		}

		template<typename T>
		std::string to_string(T&& val)
		{
			std::stringstream ss;
			if constexpr (is_string_v<std::remove_cvref_t<T>>)
			{
				ss << "'" << val << "'";
			}
			else if constexpr (is_container_v<std::remove_cvref_t<T>>)
			{
				ss << "{";
				std::for_each(val.begin(), val.end(), [&](auto iter)
							  {
								  if constexpr (is_byte_v<decltype(iter)>)
									  ss << static_cast<char>(iter) << ",";
								  else
								  {
									  ss << "{";

									  for_each(val, [&ss](const std::string& name, auto value)
											   {
												   ss << value << ",";
											   });

									  auto result = ss.str();
									  result.erase(result.size() - 1);
									  result.append("},");
								  }
							  });

				auto result = ss.str();
				result.erase(result.size() - 1);
				result.append("}");

				return result;
			}
			else if constexpr (is_byte_v<std::remove_cvref_t<T>>)
			{
				ss << static_cast<char>(std::forward<T>(val));
			}
			else if constexpr (std::is_trivial_v<std::remove_reference_t<T>>)
			{
				ss << val;
			}
			else if constexpr (is_variant_v<std::remove_cvref_t<T>>)
			{
				//constexpr std::size_t index = std::forward<T>(val).index();
				//ss << detail::to_string(std::get<index>(std::forward<T>(val)));
				/*auto result = *///for_each_variant(std::forward<T>(val),ss);

				/*ss << detail::to_string(std::get<decltype(result)>(std::forward<T>(val)));*/
			}
			else
			{
				ss << val;
			}

			return ss.str();
		}

		template<typename T>
		auto cast(const char* val)
		{
			std::stringstream ss;
			ss << val;

			T t;
			ss >> t;

			return t;
		}

		template<typename T, std::size_t... I>
		auto to_struct_impl(const MYSQL_ROW& row, std::index_sequence<I...>)
		{
			return T{ cast<decltype(reflect::get<I>(T{})) > (row[I])... };
		}

		template<typename T>
		auto to_struct(const MYSQL_ROW& row)
		{
			return detail::template to_struct_impl<T>(row, std::make_index_sequence<reflect::rf_size_v<T>>{});
		}

		template<const std::string_view&... strs>
		struct concat
		{
			constexpr static auto impl() noexcept
			{
				constexpr auto len = (strs.size() + ... + 0);
				std::array<char, len + 1> arr{};

				auto f = [i = 0, &arr](auto const& str) mutable
				{
					for (auto s : str)
						arr[i++] = s;

					return arr;
				};

				(f(strs), ...);

				arr[len] = '\0';

				return arr;
			}

			static constexpr auto arr = impl();

			static constexpr std::string_view value{ arr.data(), arr.size() - 1 };
		};

		template<std::string_view const&... strs>
		constexpr static auto concat_v = concat<strs...>::value;

		std::string to_uft8(const std::string& str)
		{
			std::vector<wchar_t> buff(str.size());

#ifdef _MSC_VER
			std::locale loc("zh-CN");
#else
			std::locale loc("zh_CN.GB18030");
#endif

			wchar_t* wnext = nullptr;
			const char* next = nullptr;
			mbstate_t state{};

			int res = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(loc).in(state, str.data(), str.data() + str.size(), next, buff.data(), buff.data() + buff.size(), wnext);
			if (res != std::codecvt_base::ok)
				return "";

			std::wstring_convert<std::codecvt_utf8<wchar_t>> cuft8;

			return cuft8.to_bytes(std::wstring(buff.data(), wnext));
		}

		std::string to_gbk(const std::string& str)
		{
			std::vector<char> buff(str.size() * 2);

			std::wstring_convert<std::codecvt_utf8<wchar_t>> cutf8;
			std::wstring wtmp = cutf8.from_bytes(str);

#ifdef _MSC_VER
			std::locale loc("zh-CN");
#else
			std::locale loc("zh_CN.GB18030");
#endif

			const wchar_t* wnext = nullptr;
			char* next = nullptr;

			mbstate_t state{};
			int res = std::use_facet<std::codecvt<wchar_t, char, mbstate_t>>(loc).out(state, wtmp.data(), wtmp.data() + wtmp.size(), wnext, buff.data(), buff.data() + buff.size(), next);
			if (res != std::codecvt_base::ok)
				return {};

			return std::string(buff.data(), next);
		}
	}
}