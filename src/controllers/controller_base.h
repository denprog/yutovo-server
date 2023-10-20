#ifndef __BASE_CONTROLLER_H__
#define __BASE_CONTROLLER_H__

#include <drogon/HttpController.h>
#include <filesystem>
#include <yutovo_logger/logger.h>

using namespace drogon;
using namespace yutovo;

namespace yutovo_server
{

namespace fs = std::filesystem;

class ControllerBase
{
public:
    ControllerBase();
    
protected:
    void SendOk(std::function<void (const HttpResponsePtr &)>& callback);
    void SendOk(std::function<void (const HttpResponsePtr &)>& callback, const std::string& document_id);
    void SendOkTokens(std::function<void (const HttpResponsePtr &)>& callback, const std::string& login, const std::string& access_uuid, 
        const std::string& refresh_uuid, const std::string& user_session, trantor::Date access_expires, trantor::Date refresh_expires, 
        trantor::Date session_expires);
    void SendJson(std::function<void (const HttpResponsePtr &)>& callback, const Json::Value& json);
    void SendJson(std::function<void (const HttpResponsePtr &)>& callback, const std::string& json);
    void SendFile(std::function<void (const HttpResponsePtr &)>& callback, const fs::path& path);
    void SendError(const HttpStatusCode status_code, const char* description, std::function<void (const HttpResponsePtr &)>& callback);

    bool ParseRefreshToken(const std::string& refresh_token, std::string& refresh_uuid, std::string& login, 
        std::function<void (const HttpResponsePtr &)>& callback);
    
    bool ParseId(const std::string& id_str, std::vector<int>& id);

    void SetDocumentCookie(const std::string& document_id, HttpResponsePtr resp);

    bool AddDocument(const std::string& user_id, std::string& document_id);

protected:
    int session_expires = 0; //session without user, in seconds, after last using
    std::string public_key, private_key;
    int access_token_expires = 60 * 2; //seconds
    int refresh_token_expires = 60 * 60 * 24; //seconds
    Logger* logger = Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log", "server", true, true);
};
}

#endif
