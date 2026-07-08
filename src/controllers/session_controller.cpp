/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "session_controller.h"
#include "../logic/clear_db.h"
#include <drogon/Session.h>
#include <drogon/plugins/RealIpResolver.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace yutovo_server
{

//SessionController

SessionController::SessionController()
{
}

void SessionController::Root(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param, std::string ref)
{
    std::string session_id;
    GetSessionId(req, callback, session_id);

    auto p = req->path();
    GetLogger(session_id)->SetLevel(yutovo::LogLevel::LEVEL_DEBUG);
    if (!ref.empty())
    {
        GetLogger(session_id)->Debug("Request root: path={}, ref={}, ip={}", p, ref, drogon::plugin::RealIpResolver::GetRealAddr(req).toIp());
        ref_logger->Info("Request: ref={}, session_id={}, ip={}, Accept-Language={}, Host={}, Referer={}, User-Agent={}", ref, session_id, 
            drogon::plugin::RealIpResolver::GetRealAddr(req).toIp(), req->getHeader("Accept-Language"), req->getHeader("Host"), req->getHeader("Referer"), 
            req->getHeader("User-Agent"));
    }
    else
        GetLogger(session_id)->Debug("Request root: path={}, ip={}", p, drogon::plugin::RealIpResolver::GetRealAddr(req).toIp());

    SessionPtr session = req->session();
    if (!session->get<bool>("log_updated"))
    {
        auto accept_language = req->getHeader("Accept-Language");
        auto host = req->getHeader("Host");
        auto referer = req->getHeader("Referer");
        auto user_agent = req->getHeader("User-Agent");
        GetLogger(session_id)->Debug("Accept-Language={}, Host={}, Referer={}, User-Agent={}", accept_language, host, referer, user_agent);
        if (!accept_language.empty() && !host.empty() && !referer.empty() && !user_agent.empty())
            session->insert("log_updated", true);
    }

    if (req->path() == "/")
    {
        orm::DbClientPtr db = app().getDbClient();
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
                        SetSessionCookie(session_id, resp);
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
                    if (!AddDocument(req, "-1", document_id, name, 0, callback))
                        return;
                }
                else
                    document_id = std::to_string(d);
            }

            if (!AddSession(document_id, session_id))
            {
                GetLogger(session_id)->Error("Database error: Error inserting a session");
                SendError(k500InternalServerError, "Error inserting a session", session_id, callback);
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
            SendError(k500InternalServerError, e.base().what(), session_id, callback);
            return;
        }
    }

    std::string r = HttpAppFramework::instance().getDocumentRoot();
    SendStaticFile(req, callback, session_id, r, param);
}

void SessionController::Assets(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
    {
        GetLogger("")->Error("Session error");
        SendError(k500InternalServerError, "session_id error", "", callback);
        return;
    }

    auto p = req->path();
    GetLogger(session_id)->Debug("Request assets: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    SendStaticFile(req, callback, session_id, r, p);
}

void SessionController::Icons(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
    {
        GetLogger("")->Error("Session error");
        SendError(k500InternalServerError, "session_id error", "", callback);
        return;
    }

    auto p = req->path();
    GetLogger(session_id)->Debug("Request icons: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    SendStaticFile(req, callback, session_id, r, p);
}

void SessionController::Images(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
    {
        GetLogger("")->Error("Session error");
        SendError(k500InternalServerError, "session_id error", "", callback);
        return;
    }

    auto p = req->path();
    GetLogger(session_id)->Debug("Request images: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    SendStaticFile(req, callback, session_id, r, p);
}

void SessionController::UserDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
    {
        GetLogger("")->Error("Session error");
        SendError(k500InternalServerError, "session_id error", "", callback);
        return;
    }

    auto p = req->path();
    GetLogger(session_id)->Info("Request user document: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    HttpAppFramework& inst = HttpAppFramework::instance();
    p = inst.getHomePage();

    if (param.find(".") == std::string::npos)
    {
        if (session_id.empty())
        {
            if (!AddSession(param, session_id))
            {
                logger->Error("Database error: Error inserting a session");
                SendError(k500InternalServerError, "Error inserting a session", session_id, callback);
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
                SetDocumentCookie(param, session_id, resp);
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
            SendError(k500InternalServerError, e.base().what(), session_id, callback);
            return;
        }

        SessionPtr session = req->session();
        session->insert("document_id", param);
        session->insert("session_id", session_id);

        auto resp = HttpResponse::newFileResponse(r + p);
        SetDocumentCookie(param, session_id, resp);
        SetSessionCookie(session_id, resp);
        callback(resp);
        return;
    }

    SendFile(req, callback, r + param);
}

void SessionController::LibraryDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string path)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
    {
        GetLogger("")->Error("Session error");
        SendError(k500InternalServerError, "session_id error", "", callback);
        return;
    }

    GetLogger(session_id)->Debug("Request library document: path={}", path);

    auto p = req->path();
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    HttpAppFramework& inst = HttpAppFramework::instance();
    GetLogger(session_id)->Info("Request library document: request={}, path={}", p, path);
    p = inst.getHomePage();

    std::replace(path.begin(), path.end(), '\\', '/');

    fs::path file_path;
    try
    {
        fs::path canonical_lib = fs::canonical(library_path);
        file_path = fs::canonical(canonical_lib / path);
        if (!file_path.string().starts_with(canonical_lib.string()))
        {
            GetLogger(session_id)->Error("Library document path not allowed: {}", path);
            auto resp = HttpResponse::newFileResponse(r + p);
            resp->setStatusCode(k404NotFound);
            SetSessionCookie(session_id, resp);
            callback(resp);
            return;
        }
    }
    catch (const std::exception& ex)
    {
        GetLogger(session_id)->Error("Library document path not found: {}", path);
        auto resp = HttpResponse::newFileResponse(r + p);
        resp->setStatusCode(k404NotFound);
        SetSessionCookie(session_id, resp);
        callback(resp);
        return;
    }

    if (!fs::exists(file_path))
    {
        GetLogger(session_id)->Error("Library document not found: {}", path);
        auto resp = HttpResponse::newFileResponse(r + p);
        resp->setStatusCode(k404NotFound);
        SetSessionCookie(session_id, resp);
        callback(resp);
        return;
    }

    if (session_id.empty())
    {
        if (!AddSession("-1", session_id))
        {
            GetLogger(session_id)->Error("Database error: Error inserting a session");
            SendError(k500InternalServerError, "Error inserting a session", session_id, callback);
            return;
        }

        SessionPtr session = req->session();
        session->insert("session_id", session_id);
    }

    SessionPtr session = req->session();
    session->insert("session_id", session_id);

    auto resp = HttpResponse::newFileResponse(r + p);
    SetSessionCookie(session_id, resp);
    SetDocumentCookie(path, session_id, resp);
    callback(resp);
}

void SessionController::Downloads(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string param)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
    {
        GetLogger("")->Error("Session error");
        SendError(k500InternalServerError, "session_id error", "", callback);
        return;
    }

    auto p = req->path();
    GetLogger(session_id)->Info("Request downloads: path={}", p);
    std::string r = HttpAppFramework::instance().getDocumentRoot();

    try
    {
        fs::path canonical_root = fs::canonical(r);
        std::string rel = p;
        if (!rel.empty() && rel.front() == '/')
            rel.erase(0, 1);
        fs::path file_path = fs::canonical(canonical_root / rel);
        std::string root_prefix = canonical_root.string();
        if (root_prefix.empty() || root_prefix.back() != '/')
            root_prefix += '/';
        if (!file_path.string().starts_with(root_prefix))
        {
            GetLogger(session_id)->Error("Download path not allowed: {}", p);
            SendError(k404NotFound, "Path not found", session_id, callback);
            return;
        }

        auto resp = HttpResponse::newFileResponse(file_path.c_str());
        downloads_logger->Info("Request download: path={}, session_id={}, ip={}, result={}", p, session_id, drogon::plugin::RealIpResolver::GetRealAddr(req).toIp(),
            (int)resp->getStatusCode());
        callback(resp);
    }
    catch (const std::exception& ex)
    {
        GetLogger(session_id)->Error("Download file path not found: {}", p);
        SendError(k404NotFound, "Path not found", session_id, callback);
    }
}

bool SessionController::SendStaticFile(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>& callback,
    const std::string& session_id, const fs::path& root, const std::string& relative)
{
    try
    {
        fs::path canonical_root = fs::canonical(root);
        std::string rel = relative;
        if (!rel.empty() && rel.front() == '/')
            rel.erase(0, 1);
        fs::path file_path = fs::canonical(canonical_root / rel);
        std::string root_prefix = canonical_root.string();
        if (root_prefix.empty() || root_prefix.back() != '/')
            root_prefix += '/';
        if (!file_path.string().starts_with(root_prefix))
        {
            GetLogger(session_id)->Error("Static file path not allowed: {}", relative);
            SendError(k404NotFound, "Path not found", session_id, callback);
            return false;
        }
        SendFile(req, callback, file_path);
        return true;
    }
    catch (const std::exception& ex)
    {
        GetLogger(session_id)->Error("Static file path not found: {}", relative);
        SendError(k404NotFound, "Path not found", session_id, callback);
        return false;
    }
}

}
