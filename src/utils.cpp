#include "utils.h"

namespace yutovo_server
{

yutovo::Logger* logger = yutovo::Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log/yutovo_server", "server", true, true);

yutovo::Logger* GetLogger(const std::string& session_id)
{
    if (session_id.empty())
        return logger;
    return yutovo::Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log/yutovo_server/sessions/" + session_id, "server", true, true);
}

}
