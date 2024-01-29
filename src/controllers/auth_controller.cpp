#include "auth_controller.h"
#include "../logic/clear_db.h"
#include <jwt-cpp/jwt.h>
#include <fstream>
#include <system_error>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace yutovo_server
{

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
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    if (!json->isMember("login") || !(*json)["login"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", callback);
        return;
    }

    auto login = (*json)["login"].asString();
    SessionPtr session = req->session();
    logger->Info("UnRegister request: login={}", login);
    orm::DbClientPtr db = app().getDbClient();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty() || user_id == "-1")
    {
        SendError(k400BadRequest, "User not found", callback);
        return;
    }

    try
    {
        orm::Result result = db->execSqlSync("delete from users where login=$1", login);
        if (result.affectedRows() == 0)
        {
            SendError(k404NotFound, "Login not found", callback);
            return;
        }
        db->execSqlSync("delete from user_sessions where user_id=$1", user_id);
        session->erase("user_id");
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
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    if (!json->isMember("login") || !(*json)["login"].isString() || !json->isMember("password") || !(*json)["password"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", callback);
        return;
    }

    auto login = (*json)["login"].asString();
    auto password = (*json)["password"].asString();
    bool load_last = true;
    if (json->isMember("load_last") && (*json)["load_last"].isBool())
        load_last = (*json)["load_last"].asBool();

    logger->Info("Login request: name={}, password={}", login, password);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        ClearDbTurnOff t; //skip the clear db circles for a while

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

        std::string document_id = "-1";
        std::string name;
        std::string session_id = req->getCookie("session_id");
        if (session_id.empty())
            session_id = session->get<std::string>("session_id");
        auto session_expires_date = trantor::Date::now().after(session_expires);
        if (!session_id.empty())
        {
            if (load_last)
            {
                result = db->execSqlSync("select document_id from user_sessions where session_id=$1", session_id);
                if (result.size() > 0)
                {
                    auto row = result[0];
                    document_id = row["document_id"].as<std::string>();
                }
            }
            if (document_id == "-1")
            {
                int d = GetFirstEmptyDocument(user_id);
                if (d == -1)
                {
                    if (!AddDocument(user_id, document_id, name))
                    {
                        logger->Error("Database error: Error inserting a document");
                        SendError(k500InternalServerError, "Error inserting a document", callback);
                        return;
                    }
                }
                else
                    document_id = std::to_string(d);
            }

            //if a user has many logins, a session may have another login
            db->execSqlSync("delete from user_sessions where session_id=$1", session_id);
            //the user session starts to have an owner
            db->execSqlSync("insert into user_sessions (session_id, user_id, expire_time, document_id) values ($1, $2, $3, $4)", 
                session_id, user_id, session_expires_date.secondsSinceEpoch(), document_id);
        }

        SendOkTokens(callback, login, access_uuid, refresh_uuid, session_id, access_expires, refresh_expires, session_expires_date, 
            document_id, name);
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
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    std::string refresh_token = req->getCookie("refresh_token");
    std::string session_id = session->get<std::string>("session_id");

    if (!json->isMember("login") || !(*json)["login"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", callback);
        return;
    }

    auto login = (*json)["login"].asString();
    std::string refresh_uuid;
    if (!ParseRefreshToken(refresh_token, refresh_uuid, login, callback))
        return;

    logger->Info("Logout request: login={}", login);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        orm::Result result = db->execSqlSync("update user_sessions set user_id=-1 where session_id=$1", session_id);
        result = db->execSqlSync("delete from refresh_sessions where user_id=$1 and refresh_uuid=$2", user_id, refresh_uuid);
        if (result.affectedRows() > 0)
            SendOk(callback);
        else
            SendError(k401Unauthorized, "Login or password are incorrect", callback);
        session->erase("user_id");
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

        std::string session_id = req->getCookie("session_id");
        if (!session_id.empty())
            UpdateSessionTime(session_id);

        SessionPtr session = req->session();
        session->insert("session_id", session_id);

        auto session_expires_date = trantor::Date::now().after(session_expires);
        SendOkTokens(callback, login, access_uuid, refresh_uuid, session_id, access_expires, refresh_expires, session_expires_date);
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

void AuthController::UpdateSessionTime(const std::string& session_id)
{
    orm::DbClientPtr db = app().getDbClient();
    trantor::Date session_expires_date = trantor::Date::now().after(session_expires);
    db->execSqlSync("update user_sessions set expire_time=$1 where session_id=$2", session_expires_date.secondsSinceEpoch(), session_id);
}

}
