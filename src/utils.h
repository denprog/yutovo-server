#ifndef __UTILS_H__
#define __UTILS_H__

#include <yutovo_logger/logger.h>

namespace yutovo_server
{
extern yutovo::Logger* logger;

yutovo::Logger* GetLogger(const std::string& session_id);

bool IsGuid(const std::string& str);
}

#endif
