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
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->SetLevel((int)trantor::Logger::logLevel());
    GetLogger(session_id)->Debug("Request root: path={}", p);
    if (req->path() == "/")
    {
        orm::DbClientPtr db = app().getDbClient();
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

                    callback(resp);
                    return;
                }
            }

            std::string document_id = "-1";
            std::string name;
            if (!user_id.empty())
            {
                int d = GetFirstEmptyDocument(user_id);
                if (d == -1)
                {
                    //create new document and session for a registered user
                    if (!AddDocument("-1", document_id, name))
                    {
                        GetLogger(session_id)->Error("Database error: Error inserting a document");
                        SendError(k500InternalServerError, "Error inserting a document", callback);
                        return;
                    }
                }
                else
                    document_id = std::to_string(d);
            }

            if (!AddSession(document_id))
            {
                GetLogger(session_id)->Error("Database error: Error inserting a session");
                SendError(k500InternalServerError, "Error inserting a session", callback);
                return;
            }

            session->insert("document_id", document_id);
            session->insert("session_id", session_id);

            if (user_id.empty())
            {
                //a non-registered user doesn't have a document in the DB
                std::string r = HttpAppFramework::instance().getDocumentRoot();
                auto resp = HttpResponse::newFileResponse(r + "/index.html");
                SetSessionCookie(session_id, resp);
                callback(resp);
                return;
            }

            //redirect to the document
            GetLogger(session_id)->Debug("Redirect {}", "/document/" + document_id);
            auto resp = HttpResponse::newRedirectionResponse("/document/" + document_id);
            SetSessionCookie(session_id, resp);
            callback(resp);
            return;
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
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
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Debug("Request images: path={}", p);
    GetLogger(session_id)->Debug("Request assets: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

void SessionController::Icons(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Debug("Request images: path={}", p);
    GetLogger(session_id)->Debug("Request icons: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

void SessionController::Images(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Debug("Request images: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

void SessionController::UserDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("Request user document: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    HttpAppFramework& inst = HttpAppFramework::instance();
    p = inst.getHomePage();

    if (param.find(".") == std::string::npos)
    {
        if (session_id.empty())
        {
            if (!AddSession(param))
            {
                logger->Error("Database error: Error inserting a session");
                SendError(k500InternalServerError, "Error inserting a session", callback);
                return;
            }

            SessionPtr session = req->session();
            session->insert("session_id", session_id);
        }
        
        orm::DbClientPtr db = app().getDbClient();
        try
        {
            //check if the document exists
            orm::Result result = db->execSqlSync("select document_id from user_documents where document_id=$1", param);
            if (result.size() == 0)
            {
                GetLogger(session_id)->Error("Document not found: {}", param);
                std::string r = HttpAppFramework::instance().getDocumentRoot();
                auto resp = HttpResponse::newFileResponse(r + "/index.html");
                SetDocumentCookie(param, resp);
                resp->setStatusCode(k404NotFound);
                SetSessionCookie(session_id, resp);
                callback(resp);
                return;
            }

            db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", param, session_id);
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
            return;
        }

        SessionPtr session = req->session();
        session->insert("document_id", param);
        session->insert("session_id", session_id);

        auto resp = HttpResponse::newFileResponse(r + p);
        SetDocumentCookie(param, resp);
        SetSessionCookie(session_id, resp);
        callback(resp);
        return;
    }

    auto resp = HttpResponse::newFileResponse(r + param);
    callback(resp);
}

void SessionController::LibraryDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("Request library document: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    HttpAppFramework& inst = HttpAppFramework::instance();
    p = inst.getHomePage();

    if (param.find(".yut") != std::string::npos || param.find("..") != std::string::npos || param.find(".") == std::string::npos)
    {
        if (session_id.empty())
        {
            if (!AddSession(param))
            {
                GetLogger(session_id)->Error("Database error: Error inserting a session");
                SendError(k500InternalServerError, "Error inserting a session", callback);
                return;
            }

            SessionPtr session = req->session();
            session->insert("session_id", session_id);
        }
        
        std::replace(param.begin(), param.end(), '\\', '/');
        if (!param.ends_with(".yut"))
            param += fs::path(".yut");
        fs::path path = fs::path(library_path + param);
        if (!fs::exists(path))
        {
            GetLogger(session_id)->Error("Library document not found: {}", param);
            std::string r = HttpAppFramework::instance().getDocumentRoot();
            auto resp = HttpResponse::newFileResponse(r + "/index.html");
            SetDocumentCookie(param, resp);
            SetSessionCookie(session_id, resp);
            resp->setStatusCode(k404NotFound);
            callback(resp);
            return;
        }

        SessionPtr session = req->session();
        session->insert("session_id", session_id);

        auto resp = HttpResponse::newFileResponse(r + p);
        SetSessionCookie(session_id, resp);
        callback(resp);
        return;
    }

    auto resp = HttpResponse::newFileResponse(r + param);
    callback(resp);
}

void SessionController::Downloads(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    auto p = req->path();
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("Request downloads: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    auto resp = HttpResponse::newFileResponse(r + p);
    callback(resp);
}

}
