#include "session_controller.h"
#include "../logic/clear_db.h"
#include <drogon/Session.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace yutovo_server
{

//SessionController

SessionController::SessionController()
{
}

void SessionController::Root(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    auto p = req->path();
    logger->Info("Request root: path={}", p);

    if (req->path() == "/")
    {
        orm::DbClientPtr db = app().getDbClient();
        std::string user_session = req->getCookie("user_session");
        try
        {
            ClearDbTurnOff t;

            if (!user_session.empty())
            {
                //find user session
                orm::Result result = db->execSqlSync("select document_id from user_sessions where session_id=$1", user_session);
                if (result.size() > 0)
                {
                    //redirect to the document
                    auto row = result[0];
                    std::string document_id = row["document_id"].as<std::string>();
                    auto resp = HttpResponse::newRedirectionResponse("/document/" + document_id);

                    SetDocumentCookie(document_id, resp);

                    callback(resp);
                    return;
                }
            }

            //create new document and session
            orm::Result result = db->execSqlSync("insert into user_documents (user_id, document) values (-1, '{}') returning document_id");
            if (result.affectedRows() == 0)
            {
                logger->Error("Database error: Error inserting a document");
                SendError(k500InternalServerError, "Error inserting a document", callback);
                return;
            }

            auto row = result[0];
            std::string document_id = row["document_id"].as<std::string>();

            user_session = std::string(boost::uuids::to_string(boost::uuids::random_generator()()));
            trantor::Date session_expires_date = trantor::Date::now().after(session_expires);
            result = db->execSqlSync("insert into user_sessions (session_id, expire_time, document_id) values ($1, $2, $3)", user_session, 
                session_expires_date.secondsSinceEpoch(), document_id);
            if (result.affectedRows() == 0)
            {
                logger->Error("Database error: Error inserting a session");
                SendError(k500InternalServerError, "Error inserting a session", callback);
                return;
            }

            SessionPtr session = req->session();
            session->insert("document_id", document_id);

            drogon::Cookie session_cookie("user_session", user_session);
            session_cookie.setHttpOnly(true);
            session_cookie.setPath("/");
            session_cookie.setExpiresDate(session_expires_date);

            //redirect to the document
            auto resp = HttpResponse::newRedirectionResponse("/document/" + document_id);
            resp->addCookie(session_cookie);
            SetDocumentCookie(document_id, resp);
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

void SessionController::Assets(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    logger->Info("Request assets: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

void SessionController::Icons(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    logger->Info("Request icons: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

void SessionController::Images(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    logger->Info("Request images: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

void SessionController::Document(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    logger->Info("Request document: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    HttpAppFramework& inst = HttpAppFramework::instance();
    p = inst.getHomePage();

    if (param.find(".") == std::string::npos)
    {
        orm::DbClientPtr db = app().getDbClient();
        try
        {
            //check if the document exists
            orm::Result result = db->execSqlSync("select document_id from user_documents where document_id=$1", param);
            if (result.size() == 0)
            {
                logger->Error("Document not found: {}", param);
                SendError(k404NotFound, "Document not found", callback);
                return;
            }
        }
        catch (const orm::DrogonDbException& e)
        {
            logger->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
            return;
        }

        SessionPtr session = req->session();
        session->insert("document_id", param);

        auto resp = HttpResponse::newFileResponse(r + p);
        SetDocumentCookie(param, resp);
        callback(resp);
        return;
    }

    auto resp = HttpResponse::newFileResponse(r + param);
    callback(resp);
}

}
