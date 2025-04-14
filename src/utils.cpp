#include "utils.h"
#include <regex>

namespace yutovo_server
{

yutovo::Logger* logger = yutovo::Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log/yutovo_server", "server", true, true);

yutovo::Logger* GetLogger(const std::string& session_id)
{
    if (session_id.empty())
        return logger;
    return yutovo::Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log/yutovo_server/sessions/" + session_id, "server", true, true);
}

yutovo::Logger* GetSolverLogger(const std::string& solver_id)
{
    return yutovo::Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log/yutovo_server/solver/" + solver_id, "solver", true, true);
}

bool IsGuid(const std::string& str)
{
    static std::regex guid("(^([0-9A-Fa-f]{8}[-]?[0-9A-Fa-f]{4}[-]?[0-9A-Fa-f]{4}[-]?[0-9A-Fa-f]{4}[-]?[0-9A-Fa-f]{12})$)");
    return std::regex_match(str, guid);
}

}
