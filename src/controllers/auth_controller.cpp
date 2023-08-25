#include "auth_controller.h"
#include <jwt-cpp/jwt.h>
#include <fstream>
#include <system_error>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace yutovo_server
{

//LoginFilter

LoginFilter::LoginFilter()
{
    std::ifstream key_file("server_key");
    if (!key_file.is_open())
        throw std::system_error(ENOENT, std::generic_category(), "Key file not open");
    
    std::stringstream ss;
    ss << key_file.rdbuf();
    private_key = ss.str();
}

void LoginFilter::doFilter(const HttpRequestPtr& req, FilterCallback&& not_valid_callback, FilterChainCallback&& valid_callback)
{
    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string access_token = req->getHeader("access_token");

    try
    {
        auto decoded = jwt::decode(access_token);
        auto login = decoded.get_payload_claim("login").to_json().to_str();
        if (login != session->get<std::string>("login"))
        {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k401Unauthorized);
            not_valid_callback(resp);
            return;
        }

        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::rs256("", private_key, "", "")).with_issuer("auth0");
        verifier.verify(decoded);
    }
    catch (std::invalid_argument& ex)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        not_valid_callback(resp);
        return;
    }
    catch (jwt::error::claim_not_present_exception& ex)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        not_valid_callback(resp);
        return;
    }
    catch (jwt::token_verification_exception& ex)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k403Forbidden);
        not_valid_callback(resp);
        return;
    }

    valid_callback();
}

//AuthController

AuthController::AuthController()
{
    const Json::Value& v = app().getCustomConfig();
    session_expires = v.get("user_session_expire_timeout", 84600).asInt();
}

void AuthController::Register(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, User&& user)
{
    logger->Info("Register request: login={}, email={}, password={}", user.login, user.email, user.password);
    orm::DbClientPtr db = app().getDbClient();
    if (user.login.empty() || user.email.empty() || user.password.empty())
    {
        SendError(k400BadRequest, "Fields must not be empty", callback);
        return;
    }

    try
    {
        //check if such login already exists
        orm::Result result = db->execSqlSync("select 1 from users where login=$1", user.login);
        if (result.size() > 0)
        {
            SendError(k409Conflict, "Login already exists", callback);
            return;
        }

        //insert new user
        result = db->execSqlSync("insert into users (login, password, email) values ($1, $2, $3)", user.login, user.password, user.email);
        if (result.size() == 0)
        {
            SendOk(callback);
        }
        else
        {
            logger->Error("Database error: {}", "Error of insert");
            SendError(k500InternalServerError, "Error of insert", callback);
        }
    } 
    catch (const orm::DrogonDbException& e)
    {
        logger->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void AuthController::UnRegister(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    auto json = req->getJsonObject();
    auto login = (*json)["login"].asString();
    SessionPtr session = req->session();
    logger->Info("UnRegister request: login={}", login);
    orm::DbClientPtr db = app().getDbClient();
    std::string user_id = session->get<std::string>("user_id");

    try
    {
        orm::Result result = db->execSqlSync("delete from users where login=$1", login);
        if (result.affectedRows() == 0)
        {
            SendError(k404NotFound, "Login not found", callback);
            return;
        }
        db->execSqlSync("delete from user_sessions where user_id=$1", user_id);
        session->insert("user_id", "");
        SendOk(callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        logger->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void AuthController::Login(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    auto json = req->getJsonObject();
    auto login = (*json)["login"].asString();
    auto password = (*json)["password"].asString();
    logger->Info("Login request: name={}, password={}", login, password);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        orm::Result result = db->execSqlSync("select user_id from users where login=$1 and password=$2", login, password);
        if (result.size() == 0)
        {
            SendError(k401Unauthorized, "Login or password are incorrect", callback);
            return;
        }

        //create a session with refresh and access tokens
        SessionPtr session = req->session();
        session->insert("login", login);

        std::string refresh_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
        std::string access_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
        trantor::Date access_expires = trantor::Date::now().after(access_token_expires);
        trantor::Date refresh_expires = trantor::Date::now().after(refresh_token_expires);

        auto row = result[0];
        std::string user_id = row["user_id"].as<std::string>();
        result = db->execSqlSync("insert into refresh_sessions (user_id, refresh_uuid, expire_time) values ($1, $2, $3)", 
            user_id, refresh_uuid, refresh_expires.secondsSinceEpoch());
        session->insert("user_id", user_id);

        std::string user_session = req->getCookie("user_session");
        auto session_expires_date = trantor::Date::now().after(session_expires);
        if (!user_session.empty())
        {
            //if a user has many logins, a session may have another login
            db->execSqlSync("delete from user_sessions where session_id=$1", user_session);
            //the user session starts to have an owner
            db->execSqlSync("insert into user_sessions (session_id, user_id, expire_time) values ($1, $2, $3)", user_session, user_id, 
                session_expires_date.secondsSinceEpoch());
        }

        SendOkTokens(callback, login, access_uuid, refresh_uuid, user_session, access_expires, refresh_expires, session_expires_date);
    }
    catch (const orm::DrogonDbException& e)
    {
        logger->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void AuthController::Logout(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    auto json = req->getJsonObject();
    auto login = (*json)["login"].asString();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    std::string refresh_token = req->getCookie("refresh_token");

    std::string refresh_uuid;
    if (!ParseRefreshToken(refresh_token, refresh_uuid, login, callback))
        return;

    logger->Info("Logout request: login={}", login);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        orm::Result result = db->execSqlSync("delete from refresh_sessions where user_id=$1 and refresh_uuid=$2", user_id, refresh_uuid);
        if (result.affectedRows() > 0)
            SendOk(callback);
        else
            SendError(k401Unauthorized, "Login or password are incorrect", callback);
        session->clear();
    }
    catch (const orm::DrogonDbException& e)
    {
        logger->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void AuthController::RefreshToken(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string refresh_token = req->getCookie("refresh_token");
    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();

    try
    {
        std::string refresh_uuid;
        std::string login;
        if (!ParseRefreshToken(refresh_token, refresh_uuid, login, callback))
            return;

        logger->Info("RefreshToken request: login={}, refresh_uuid={}", login, refresh_uuid);

        orm::Result result = db->execSqlSync("select user_id from users where login=$1", login);
        if (result.size() == 0)
        {
            SendError(k401Unauthorized, "User not found", callback);
            return;
        }

        auto row = result[0];
        auto user_id = row["user_id"].as<std::string>();

        result = db->execSqlSync("delete from refresh_sessions where refresh_uuid=$1", refresh_uuid);
        if (result.affectedRows() == 0)
        {
            SendError(k401Unauthorized, "Refresh session is incorrect", callback);
            return;
        }

        refresh_uuid = std::string(boost::uuids::to_string(boost::uuids::random_generator()()));
        std::string access_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
        trantor::Date access_expires = trantor::Date::now().after(access_token_expires);
        trantor::Date refresh_expires = trantor::Date::now().after(refresh_token_expires);

        result = db->execSqlSync("insert into refresh_sessions (user_id, refresh_uuid, expire_time) values ($1, $2, $3)", 
            user_id, refresh_uuid, refresh_expires.secondsSinceEpoch());
        
        session->insert("login", login);
        session->insert("user_id", user_id);

        std::string user_session = req->getCookie("user_session");
        if (!user_session.empty())
            UpdateSessionTime(user_session);

        auto session_expires_date = trantor::Date::now().after(session_expires);
        SendOkTokens(callback, login, access_uuid, refresh_uuid, user_session, access_expires, refresh_expires, session_expires_date);
    }
    catch (const orm::DrogonDbException& e)
    {
        logger->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

#ifdef TEST
void AuthController::SetParams(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    access_token_expires = req->getOptionalParameter<int>("access_token_expires").value();
    refresh_token_expires = req->getOptionalParameter<int>("refresh_token_expires").value();
    SendOk(callback);
}
#endif

void AuthController::UpdateSessionTime(const std::string& user_session)
{
    orm::DbClientPtr db = app().getDbClient();
    trantor::Date session_expires_date = trantor::Date::now().after(session_expires);
    db->execSqlSync("update user_sessions set expire_time=$1 where session_id=$2", session_expires_date.secondsSinceEpoch(), user_session);
}

}
