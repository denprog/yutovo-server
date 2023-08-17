#ifndef __API_CONTROLLER_H__
#define __API_CONTROLLER_H__

#include <drogon/HttpController.h>
#include <yutovo_logger/logger.h>

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
    return yutovo_server::User{req.getParameter("login"), req.getParameter("email"), req.getParameter("password")};
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

class AuthController : public drogon::HttpController<AuthController>
{
public:
    AuthController();

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::Register, "/auth/register", Post);
    ADD_METHOD_TO(AuthController::UnRegister, "/auth/unregister", Post, "yutovo_server::LoginFilter");
    ADD_METHOD_TO(AuthController::Login, "/auth/login", Post);
    ADD_METHOD_TO(AuthController::Logout, "/auth/logout", Post, "yutovo_server::LoginFilter");
    ADD_METHOD_TO(AuthController::RefreshToken, "/auth/refresh-token", Post, "yutovo_server::LoginFilter");
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
    void SendOk(std::function<void (const HttpResponsePtr &)>& callback);
    void SendOkTokens(std::function<void (const HttpResponsePtr &)>& callback, const std::string& login, std::string& access_uuid, 
        std::string& refresh_uuid, trantor::Date access_expires, trantor::Date refresh_expires);
    void SendError(const HttpStatusCode status_code, const char* description, std::function<void (const HttpResponsePtr &)>& callback);

    bool GetRefreshUuid(const std::string& refresh_token, std::string& refresh_uuid, std::function<void (const HttpResponsePtr &)>& callback);

private:
    std::string public_key, private_key;
    int access_token_expires = 60 * 2; //seconds
    int refresh_token_expires = 60 * 60 * 24; //seconds
    Logger* logger = Logger::GetInstance(std::string(std::getenv("YUTOVO_DEPLOY")) + "/log", "server", true, true);
};
}

#endif
