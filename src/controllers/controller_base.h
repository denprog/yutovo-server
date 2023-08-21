#ifndef __BASE_CONTROLLER_H__
#define __BASE_CONTROLLER_H__

#include <drogon/HttpController.h>
#include <yutovo_logger/logger.h>

using namespace drogon;
using namespace yutovo;

namespace yutovo_server
{
class ControllerBase
{
public:
    ControllerBase();
    
protected:
    void SendOk(std::function<void (const HttpResponsePtr &)>& callback);
    void SendOkTokens(std::function<void (const HttpResponsePtr &)>& callback, const std::string& login, std::string& access_uuid, 
        std::string& refresh_uuid, trantor::Date access_expires, trantor::Date refresh_expires);
    void SendError(const HttpStatusCode status_code, const char* description, std::function<void (const HttpResponsePtr &)>& callback);

    bool GetRefreshUuid(const std::string& refresh_token, std::string& refresh_uuid, std::function<void (const HttpResponsePtr &)>& callback);

protected:
    std::string public_key, private_key;
    int access_token_expires = 60 * 2; //seconds
    int refresh_token_expires = 60 * 60 * 24; //seconds
    Logger* logger = Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log", "server", true, true);
};
}

#endif
