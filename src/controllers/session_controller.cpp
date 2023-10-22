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

void SessionController::Root(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    logger->Info("Request root: path={}", p);

    if (req->path() == "/")
    {
        orm::DbClientPtr db = app().getDbClient();
        std::string session_id = req->getCookie("session_id");
        SessionPtr session = req->session();
        std::string user_id = session->get<std::string>("user_id");

        try
        {
            ClearDbTurnOff t;

            if (!session_id.empty())
            {
                //find user session
                orm::Result result = db->execSqlSync("select document_id from user_sessions where session_id=$1", session_id);
                if (result.size() > 0)
                {
                    session->insert("session_id", session_id);

                    auto row = result[0];
                    std::string document_id = row["document_id"].as<std::string>();

                    if (user_id.empty() || document_id == "-1")
                    {
                        std::string r = HttpAppFramework::instance().getDocumentRoot();
                        auto resp = HttpResponse::newFileResponse(r + "/index.html");
                        callback(resp);
                        return;
                    }

                    //redirect to the document
                    session->insert("document_id", document_id);
                    auto resp = HttpResponse::newRedirectionResponse("/document/" + document_id);

                    SetDocumentCookie(document_id, resp);

                    callback(resp);
                    return;
                }
            }

            std::string document_id = "-1";
            if (!user_id.empty())
            {
                //create new document and session for a registered user
                if (!AddDocument("-1", document_id))
                {
                    logger->Error("Database error: Error inserting a document");
                    SendError(k500InternalServerError, "Error inserting a document", callback);
                    return;
                }
            }

            if (!AddSession(document_id, session_id))
            {
                logger->Error("Database error: Error inserting a session");
                SendError(k500InternalServerError, "Error inserting a session", callback);
                return;
            }

            session->insert("document_id", document_id);
            session->insert("session_id", session_id);

            drogon::Cookie session_cookie("session_id", session_id);
            session_cookie.setHttpOnly(false);
            session_cookie.setPath("/");
            session_cookie.setExpiresDate(trantor::Date::now().after(session_expires));

            if (user_id.empty())
            {
                //a non-registered user doesn't have a document in the DB
                std::string r = HttpAppFramework::instance().getDocumentRoot();
                auto resp = HttpResponse::newFileResponse(r + "/index.html");
                resp->addCookie(session_cookie);
                callback(resp);
                return;
            }

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
    auto resp = HttpResponse::newFileResponse(r + param);
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
        std::string session_id = req->getCookie("session_id");
        if (session_id.empty())
        {
            if (!AddSession(param, session_id))
            {
                logger->Error("Database error: Error inserting a session");
                SendError(k500InternalServerError, "Error inserting a session", callback);
                return;
            }
        }
        
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

            db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", param, session_id);
        }
        catch (const orm::DrogonDbException& e)
        {
            logger->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
            return;
        }

        SessionPtr session = req->session();
        session->insert("document_id", param);
        session->insert("session_id", session_id);

        auto resp = HttpResponse::newFileResponse(r + p);
        SetDocumentCookie(param, resp);
        callback(resp);
        return;
    }

    auto resp = HttpResponse::newFileResponse(r + param);
    callback(resp);
}

}
