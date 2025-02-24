#ifndef __API_CONTROLLER_H__
#define __API_CONTROLLER_H__

#include "controller_base.h"

using namespace drogon;
using namespace yutovo;

namespace yutovo_server
{
struct User
{
    std::string login;
    std::string email;
    std::string password;
    std::string name;
};
}

namespace drogon
{
template <>
inline yutovo_server::User fromRequest(const HttpRequest &req)
{
    auto json = req.getJsonObject();
    if (!json || !json->isObject())
        return yutovo_server::User{"", "", ""};
    if (!json->isMember("login") || !(*json)["login"].isString() || !json->isMember("email") || !(*json)["email"].isString() || 
        !json->isMember("password") || !(*json)["password"].isString())
    {
        return yutovo_server::User{"", "", ""};
    }
    std::string name;
    if (json->isMember("name") && (*json)["name"].isString())
        name = (*json)["name"].asString();

    return yutovo_server::User{(*json)["login"].asString(), (*json)["email"].asString(), (*json)["password"].asString(), name};
}
}

namespace yutovo_server
{

struct upload_status
{
    size_t bytes_read;
};

class AuthController : public drogon::HttpController<AuthController>, public ControllerBase
{
public:
    AuthController();

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::GetCaptcha, "/auth/get-captcha", Post);
    ADD_METHOD_TO(AuthController::SendRegisterCode, "/auth/send-register-code", Post);
    ADD_METHOD_TO(AuthController::Register, "/auth/register", Post);
    ADD_METHOD_TO(AuthController::UnRegister, "/auth/unregister", Post, "yutovo_server::LoginFilter");
    ADD_METHOD_TO(AuthController::Login, "/auth/login", Post);
    ADD_METHOD_TO(AuthController::Logout, "/auth/logout", Post, "yutovo_server::LoginFilter");
    ADD_METHOD_TO(AuthController::RefreshToken, "/auth/refresh-token", Post);
    ADD_METHOD_TO(AuthController::SetLanguage, "/auth/set-language", Post, "yutovo_server::LoginFilter");
    ADD_METHOD_TO(AuthController::GetLanguage, "/auth/get-language", Post, "yutovo_server::LoginFilter");
#ifdef TEST
    ADD_METHOD_TO(AuthController::SetParams, "/auth/set-params", Post);
#endif
    METHOD_LIST_END

    void GetCaptcha(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SendRegisterCode(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback);
    void Register(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, User&& user);
    void UnRegister(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void Login(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void Logout(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void RefreshToken(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void SetLanguage(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void GetLanguage(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);

#ifdef TEST
    void SetParams(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
#endif

private:
    void UpdateSessionTime(const std::string& session_id);
    std::string GetHash(const std::string& str, const std::string& salt);
    static size_t EmailPayload(char *ptr, size_t size, size_t nmemb, void *userp);
    bool SendEmail(const std::string& from, const std::string& to, const std::string& subject, const std::string& message);

private:
    int session_expires = 0;

    Logger* auth_logger = Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log/yutovo_server/auth", "server", true, true);

    std::string email_message;
    upload_status upload_context;
};

}

#endif
