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
    return yutovo_server::User{(*json)["login"].asString(), (*json)["email"].asString(), (*json)["password"].asString()};
}
}

namespace yutovo_server
{
class LoginFilter : public drogon::HttpFilter<LoginFilter>
{
public:
    LoginFilter();

    virtual void doFilter(const HttpRequestPtr& req, FilterCallback&& not_valid_callback, FilterChainCallback&& valid_callback) override;

private:
    std::string public_key, private_key;
    Logger* logger = Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log", "server", true, true);
};

class AuthController : public drogon::HttpController<AuthController>, public ControllerBase
{
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::Register, "/auth/register", Get);
    ADD_METHOD_TO(AuthController::UnRegister, "/auth/unregister", Get, "yutovo_server::LoginFilter");
    ADD_METHOD_TO(AuthController::Login, "/auth/login", Get);
    ADD_METHOD_TO(AuthController::Logout, "/auth/logout", Get, "yutovo_server::LoginFilter");
    ADD_METHOD_TO(AuthController::RefreshToken, "/auth/refresh-token", Get, "yutovo_server::LoginFilter");
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
};
}

#endif
