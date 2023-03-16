#pragma once
#include <boost/pfr.hpp>

namespace sqlpro
{
	namespace reflect
	{
		template<typename _Ty>
		constexpr auto rf_size_v = boost::pfr::tuple_size_v<_Ty>;

		template<std::size_t I, typename _Ty>
		constexpr decltype(auto) get(_Ty& val)
		{
			return boost::pfr::get<I, _Ty>(val);
		}

#define MAKE_REFLECT(...)	\
	template<typename _Ty>\
	static auto make_reflect_member()\
	{\
		struct reflect_member\
		{\
			inline constexpr static auto member_str()\
			{\
				return std::string_view(#__VA_ARGS__,sizeof(#__VA_ARGS__) - 1);\
			}\
			inline constexpr decltype(auto) static apply_member()\
			{\
				return reflect::template split<reflect_member,rf::template rf_size<_Ty>::value>();\
			}\
		};\
		return reflect_member{};\
	}

		template<typename T>
		constexpr std::string_view feild_name()
		{
			using namespace std::string_view_literals;
			std::size_t start_pos = 0;
			std::size_t length = 0;
#ifdef _MSC_VER
			constexpr std::string_view name = __FUNCSIG__""sv;
			start_pos = 105;
			length = name.size() - 112;
#elif __GNUC__
			constexpr std::string_view name = __PRETTY_FUNCTION__;
			start_pos = 50;
			length = name.size() - 100;
#endif
			return name.substr(start_pos, length);
		}

#define MACRO(...) __VA_ARGS__

#define REFLECT_DEFINE(...)	MACRO(__VA_ARGS__) MAKE_REFLECT(__VA_ARGS__)

		template<typename T>
		constexpr auto title()
		{
			return feild_name<T>();
		}

		template<typename T, std::size_t N>
		constexpr auto name() -> std::string_view
		{
			return std::get<N>(decltype(T::template make_reflect_member<T>())::apply_member());
		}
	}
}