#include "session_controller.h"
#include <drogon/Session.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace yutovo_server
{

//SessionController

void SessionController::Root(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    auto p = req->path();
    logger->Info("Request: path={}", p);

    HttpAppFramework& inst = HttpAppFramework::instance();
    if (req->path() == "/")
    {
        orm::DbClientPtr db = app().getDbClient();
        std::string user_session = req->getCookie("user_session");
        try
        {
            if (!user_session.empty())
            {
                //find user session
                orm::Result result = db->execSqlSync("select 1 from sessions where session_id=$1", user_session);
                if (result.size() > 0)
                {
                    //redirect to the session
                    auto resp = HttpResponse::newRedirectionResponse("/session/" + user_session);
                    callback(resp);
                    return;
                }
            }

            //create new session
            user_session = std::string(boost::uuids::to_string(boost::uuids::random_generator()()));
            trantor::Date session_expires_date = trantor::Date::now().after(session_expires);
            orm::Result result = db->execSqlSync("insert into sessions (session_id, expire_time, folder) values ($1, $2, $3)", user_session, 
                session_expires_date.secondsSinceEpoch(), "");
            if (result.affectedRows() == 0)
            {
                logger->Error("Database error: Error inserting a session");
                SendError(k500InternalServerError, "Error inserting a session", callback);
                return;
            }

            drogon::Cookie session_cookie("user_session", user_session);
            session_cookie.setHttpOnly(true);
            session_cookie.setPath("/");
            session_cookie.setExpiresDate(session_expires_date);

            //redirect to the session
            auto resp = HttpResponse::newRedirectionResponse("/session/" + user_session);
            resp->addCookie(session_cookie);
            callback(resp);
            return;
        }
        catch (const orm::DrogonDbException& e)
        {
            logger->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
            return;
        }
    }

    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

void SessionController::Session(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    logger->Info("Request: path={}", p);
    std::string folder;
    orm::DbClientPtr db = app().getDbClient();
    HttpAppFramework& inst = HttpAppFramework::instance();
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    p = inst.getHomePage();

    if (param.find(".") == std::string::npos)
    {
        try
        {
            //find user session
            orm::Result result = db->execSqlSync("select folder from sessions where session_id=$1", param);
            if (result.size() == 0)
            {
                logger->Error("Session not found: {}", param);
                SendError(k500InternalServerError, "Session not found", callback);
                return;
            }
            auto row = result[0];
            folder = row["folder"].as<std::string>();

            trantor::Date session_expires_date = trantor::Date::now().after(session_expires);
            db->execSqlSync("update sessions set expire_time=$1 where session_id=$2", session_expires_date.secondsSinceEpoch(), param);

            //update the session cookie
            drogon::Cookie session_cookie("user_session", param);
            session_cookie.setHttpOnly(true);
            session_cookie.setPath("/");
            session_cookie.setExpiresDate(session_expires_date);

            auto resp = HttpResponse::newFileResponse(r + p);
            resp->addCookie(session_cookie);
            callback(resp);
        }
        catch (const orm::DrogonDbException& e)
        {
            logger->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
        }
    }
    else
    {
        auto resp = HttpResponse::newFileResponse(r + param);
        callback(resp);
    }
}

void SessionController::Assets(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    logger->Info("Request: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

void SessionController::Icons(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    logger->Info("Request: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

void SessionController::Images(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    logger->Info("Request: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

}
