#pragma once

#include "sqlpro/service_pool.hpp"
#include <sqlpro/mysql_service.hpp>

namespace sqlpro
{
	using db_mysql = service_pool<mysql_connect>;
}
