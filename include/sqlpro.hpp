#pragma once

#include "sqlpro/service_pool.hpp"
#include <sqlpro/detail/service/mysql_connect.hpp>

namespace sqlpro
{
	using db_mysql = service_pool<mysql_connect>;
}
