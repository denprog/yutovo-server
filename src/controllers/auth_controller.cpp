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

void AuthController::Register(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, User&& user)
{
    logger->Info("Register request: login={}, email={}, password={}", user.login, user.email, user.password);
    orm::DbClientPtr db = app().getDbClient();

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
    SessionPtr session = req->session();
    std::string login = session->get<std::string>("login");
    logger->Info("UnRegister request: login={}", login);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        orm::Result result = db->execSqlSync("delete from users where login=$1", login);
        if (result.affectedRows() == 0)
            SendError(k404NotFound, "Login not found", callback);
        else
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
    auto login = req->getParameter("login");
    auto password = req->getParameter("password");
    logger->Info("Login request: name={}, password={}", login, password);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        orm::Result result = db->execSqlSync("select user_id from users where login=$1 and password=$2", login, password);
        if (result.size() > 0)
        {
            //create a session with refresh and access tokens
            SessionPtr session = req->session();
            session->insert("login", login);

            std::string refresh_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
            std::string access_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
            trantor::Date access_expires = trantor::Date::now().after(access_token_expires);
            trantor::Date refresh_expires = trantor::Date::now().after(refresh_token_expires);

            auto row = result[0];
            result = db->execSqlSync("insert into refresh_sessions (user_id, refresh_uuid, expires) values ($1, $2, $3)", 
                row["user_id"].as<std::string>(), refresh_uuid, refresh_expires.secondsSinceEpoch());
            session->insert("user_id", row["user_id"].as<std::string>());

            SendOkTokens(callback, login, access_uuid, refresh_uuid, access_expires, refresh_expires);
        }
        else
            SendError(k401Unauthorized, "Login or password are incorrect", callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        logger->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void AuthController::Logout(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    SessionPtr session = req->session();
    std::string login = session->get<std::string>("login");
    std::string user_id = session->get<std::string>("user_id");
    std::string refresh_token = req->getCookie("refresh_token");
    std::string refresh_uuid;

    if (!GetRefreshUuid(refresh_token, refresh_uuid, callback))
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
    SessionPtr session = req->session();
    std::string login = session->get<std::string>("login");
    std::string user_id = session->get<std::string>("user_id");
    std::string refresh_token = req->getCookie("refresh_token");

    logger->Info("RefreshToken request: login={}", login);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        std::string refresh_uuid;
        if (!GetRefreshUuid(refresh_token, refresh_uuid, callback))
            return;

        orm::Result result = db->execSqlSync("delete from refresh_sessions where user_id=$1 and refresh_uuid=$2", user_id, refresh_uuid);
        if (result.affectedRows() == 0)
        {
            SendError(k401Unauthorized, "Refresh session is incorrect", callback);
            return;
        }

        refresh_uuid = std::string(boost::uuids::to_string(boost::uuids::random_generator()()));
        std::string access_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
        trantor::Date access_expires = trantor::Date::now().after(access_token_expires);
        trantor::Date refresh_expires = trantor::Date::now().after(refresh_token_expires);

        result = db->execSqlSync("insert into refresh_sessions (user_id, refresh_uuid, expires) values ($1, $2, $3)", 
            user_id, refresh_uuid, refresh_expires.secondsSinceEpoch());

        SendOkTokens(callback, login, access_uuid, refresh_uuid, access_expires, refresh_expires);
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

}
