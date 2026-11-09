/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "session_controller.h"
#include "../logic/clear_db.h"
#include "../logic/ssr_html.h"
#include <ctime>
#include <vector>
#include <functional>
#include <algorithm>
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
                GetLogger("")->Error("Database error: Error inserting a session");
                SendError(k500InternalServerError, "Error inserting a session", session_id, callback);
                return;
            }

            SessionPtr session = req->session();
            session->insert("session_id", session_id);
        }
        
        orm::DbClientPtr db = app().getDbClient();
        std::string document_name, document_json;
        bool document_public = false;
        try
        {
            //check if the document exists; the document json is read only for public ones
            orm::Result result = db->execSqlSync("select name, public from user_documents where document_id=$1", param);
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

            auto row = result[0];
            document_name = row["name"].as<std::string>();
            document_public = row["public"].as<bool>();
            if (document_public)
            {
                result = db->execSqlSync("select document from user_documents where document_id=$1", param);
                document_json = result[0]["document"].as<std::string>();
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

        //generate the SEO version of the page with the document content
        if (document_public)
        {
            std::string page = MakeDocumentPage(document_json, document_name, session_id);
            if (!page.empty())
            {
                auto resp = HttpResponse::newHttpResponse();
                resp->setContentTypeString("text/html; charset=utf-8");
                resp->setBody(page);
                SetDocumentCookie(param, session_id, resp);
                SetSessionCookie(session_id, resp);
                callback(resp);
                return;
            }
        }

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

    auto ref = req->getParameter("ref");
    if (!ref.empty())
    {
        GetLogger(session_id)->Debug("Request library document: path={}, ref={}, ip={}", path, ref, drogon::plugin::RealIpResolver::GetRealAddr(req).toIp());
        ref_logger->Info("Request: ref={}, session_id={}, ip={}, Accept-Language={}, Host={}, Referer={}, User-Agent={}", ref, session_id,
            drogon::plugin::RealIpResolver::GetRealAddr(req).toIp(), req->getHeader("Accept-Language"), req->getHeader("Host"), req->getHeader("Referer"),
            req->getHeader("User-Agent"));
    }

    auto p = req->path();
    std::string r = HttpAppFramework::instance().getDocumentRoot();
    HttpAppFramework& inst = HttpAppFramework::instance();
    GetLogger(session_id)->Info("Request library document: request={}, path={}", p, path);
    p = inst.getHomePage();

    //the route capture keeps the percent encoding, while crawlers always encode
    //spaces and non-ascii names; decode before resolving the file
    path = utils::urlDecode(path);
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

    //generate the SEO version of the page with the document content
    std::string page;
    if (SsrCache::GetInstance().GetLibraryPage(file_path.string(), file_path.stem().string(), page))
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setContentTypeString("text/html; charset=utf-8");
        resp->setBody(page);
        SetSessionCookie(session_id, resp);
        SetDocumentCookie(path, session_id, resp);
        callback(resp);
        return;
    }

    auto resp = HttpResponse::newFileResponse(r + p);
    SetSessionCookie(session_id, resp);
    SetDocumentCookie(path, session_id, resp);
    callback(resp);
}

void SessionController::Sitemap(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    //no session is created: the sitemap is fetched by crawlers only; the urls belong to the requested host, the scheme comes from the proxy
    std::string scheme = req->getHeader("X-Forwarded-Proto");
    if (scheme.empty())
        scheme = "https";
    std::string host = req->getHeader("Host");
    if (host.empty())
        host = "yutovo.ru";

    std::string sitemap = MakeSitemap(scheme + "://" + host);
    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeString("application/xml; charset=utf-8");
    if (sitemap.empty())
        resp->setStatusCode(k404NotFound);
    else
        resp->setBody(sitemap);
    callback(resp);
}

std::string SessionController::MakeSitemap(const std::string& base_url)
{
    {
        std::lock_guard<std::mutex> lock(sitemap_mutex);
        auto now = std::chrono::steady_clock::now();
        auto it = sitemaps.find(base_url);
        if (it != sitemaps.end() && now - it->second.time < std::chrono::hours(1))
            return it->second.sitemap;
    }

    std::vector<fs::path> files;
    std::function<void(const fs::path&)> collect_files =
        [&](const fs::path& dir) -> void
        {
            std::error_code ec;
            std::vector<fs::path> children;
            for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
                children.push_back(it->path());
            if (ec)
                return;

            //sorted names give a stable sitemap
            std::sort(children.begin(), children.end());
            for (const auto& child : children)
            {
                if (fs::is_directory(child))
                    collect_files(child);
                else if (child.extension() == ".yut")
                    files.push_back(child);
            }
        };
    collect_files(fs::path(library_path));

    std::string sitemap =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";
    sitemap += "<url><loc>" + base_url + "/</loc></url>\n";

    const char* hex = "0123456789ABCDEF";
    for (const auto& file : files)
    {
        std::string relative = file.lexically_relative(library_path).string();

        //the document path is percent encoded, only the url unreserved characters are kept
        std::string url;
        for (char c : relative)
        {
            unsigned char u = static_cast<unsigned char>(c);
            bool unreserved = (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') ||
                c == '-' || c == '.' || c == '_' || c == '~' || c == '/';
            if (unreserved)
                url += c;
            else
            {
                url += '%';
                url += hex[u >> 4];
                url += hex[u & 0xf];
            }
        }

        //the modification date of the document file, empty on an error
        std::string lastmod;
        std::error_code ec;
        auto file_time = fs::last_write_time(file, ec);
        if (!ec)
        {
            std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::clock_cast<std::chrono::system_clock>(file_time));
            std::tm tm = {};
            gmtime_r(&time, &tm);
            char buffer[16];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
            lastmod = buffer;
        }

        sitemap += "<url><loc>" + base_url + "/library/" + url + "</loc><lastmod>" + lastmod + "</lastmod></url>\n";
    }
    sitemap += "</urlset>\n";

    std::lock_guard<std::mutex> lock(sitemap_mutex);
    sitemaps[base_url] = {std::chrono::steady_clock::now(), sitemap};
    return sitemap;
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

//Build the SPA page with the document content injected, empty on any error
std::string SessionController::MakeDocumentPage(const std::string& document_json, const std::string& name, const std::string& session_id)
{
    Json::Value document;
    Json::Reader reader;
    if (!reader.parse(document_json, document) || !document.isObject())
    {
        GetLogger(session_id)->Error("Document not parsed for the SEO page");
        return "";
    }

    std::string index_html;
    if (!SsrCache::GetInstance().GetIndexPage(index_html))
        return "";

    SsrDocument ssr(document);
    return SsrPage::MakePage(index_html, name, ssr.FirstParagraphText(160), SsrDocument::LangFromLanguage(ssr.Language()), ssr.GenerateArticle(name));
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
