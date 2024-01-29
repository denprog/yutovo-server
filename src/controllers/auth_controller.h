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

    return yutovo_server::User{(*json)["login"].asString(), (*json)["email"].asString(), (*json)["password"].asString()};
}
}

namespace yutovo_server
{
class AuthController : public drogon::HttpController<AuthController>, public ControllerBase
{
public:
    AuthController();

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::Register, "/auth/register", Post);
    ADD_METHOD_TO(AuthController::UnRegister, "/auth/unregister", Post, "yutovo_server::LoginFilter");
    ADD_METHOD_TO(AuthController::Login, "/auth/login", Post);
    ADD_METHOD_TO(AuthController::Logout, "/auth/logout", Post, "yutovo_server::LoginFilter");
    ADD_METHOD_TO(AuthController::RefreshToken, "/auth/refresh-token", Post);
#ifdef TEST
    ADD_METHOD_TO(AuthController::SetParams, "/auth/set-params", Post);
#endif
    METHOD_LIST_END

    void Register(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, User&& user);
    void UnRegister(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void Login(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void Logout(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
    void RefreshToken(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);

#ifdef TEST
    void SetParams(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback);
#endif

private:
    void UpdateSessionTime(const std::string& session_id);

private:
    int session_expires = 0;
};
}

#endif
