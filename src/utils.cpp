#include "utils.h"
#include <sstream>

namespace yutovo_server
{

yutovo::Logger* logger = yutovo::Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log/yutovo_server", "server", true, true);

yutovo::Logger* GetLogger(const std::string& session_id)
{
    if (session_id.empty())
        return logger;
    return yutovo::Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log/yutovo_server/sessions/" + session_id, "server", true, true);
}

bool IsGuid(const std::string& str)
{
    std::istringstream f(str);
    std::string s;
    int p = 0;
    while (getline(f, s, '-'))
    {
        if ((p == 0 && s.size() != 8) || ((p >= 1 && p <= 3) && s.size() != 4) || (p == 4 && s.size() != 12) || !std::all_of(s.begin(), s.end(), ::isalnum))
            return false;
        ++p;
    }
    return p == 5;
}

}
