#include "controller_base.h"
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
    logger->Info("doFilter {}", req->path());
    SessionPtr session = req->session();
    std::string access_token = req->getHeader("access_token");
    HttpResponsePtr resp;

    try
    {
        auto decoded = jwt::decode(access_token);
        auto login = decoded.get_payload_claim("login").to_json().to_str();
        if (login != session->get<std::string>("login"))
        {
            logger->Error("Unanuthorized: {}", login);
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k401Unauthorized);
            not_valid_callback(resp);
            return;
        }

        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::rs256("", private_key, "", "")).with_issuer("auth0");
        verifier.verify(decoded);
        valid_callback();
        return;
    }
    catch (std::invalid_argument& ex)
    {
        logger->Error("Wrong access token: {}", ex.what());
        resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
    }
    catch (jwt::error::claim_not_present_exception& ex)
    {
        logger->Error("Wrong access token: {}", ex.what());
        resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
    }
    catch (jwt::token_verification_exception& ex)
    {
        logger->Error("Wrong access token: {}", ex.what());
        resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k403Forbidden);
    }
    catch (...)
    {
        logger->Error("Wrong access token");
        resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k405MethodNotAllowed);
    }

    not_valid_callback(resp);
}

//ControllerBase

ControllerBase::ControllerBase()
{
    std::ifstream key_file("server_key");
    if (!key_file.is_open())
        throw std::system_error(ENOENT, std::generic_category(), "Pirvate key file not open");
    
    std::stringstream ss;
    ss << key_file.rdbuf();
    private_key = ss.str();

    std::ifstream public_key_file("server_key.pub");
    if (!public_key_file.is_open())
        throw std::system_error(ENOENT, std::generic_category(), "Public key file not open");
    
    ss.str("");
    ss << public_key_file.rdbuf();
    public_key = ss.str();

    const Json::Value& v = app().getCustomConfig();
    session_expires = v.get("session_expire_timeout", 86400).asInt();
}

void ControllerBase::SendOk(std::function<void (const HttpResponsePtr &)>& callback)
{
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_PLAIN);
    callback(resp);
}

void ControllerBase::SendOk(std::function<void (const HttpResponsePtr &)>& callback, const std::string& document_id)
{
    Json::Value r;
    r["document_id"] = document_id;
    auto resp = HttpResponse::newHttpJsonResponse(r);
    resp->setStatusCode(k200OK);
    LogJson(r);
    //SetDocumentCookie(document_id, resp);
    callback(resp);
}

void ControllerBase::SendOk(std::function<void (const HttpResponsePtr &)>& callback, const std::string& document_id, const std::string& name)
{
    Json::Value r;
    r["document_id"] = document_id;
    r["name"] = name;
    auto resp = HttpResponse::newHttpJsonResponse(r);
    resp->setStatusCode(k200OK);
    LogJson(r);
    //SetDocumentCookie(document_id, resp);
    callback(resp);
}

void ControllerBase::SendOkTokens(std::function<void (const HttpResponsePtr &)>& callback, const std::string& login, const std::string& access_uuid, 
    const std::string& refresh_uuid, const std::string& session_id, trantor::Date access_expires, trantor::Date refresh_expires, 
    trantor::Date session_expires, const std::string document_id, const std::string name)
{
    auto access_token = jwt::create().
        set_issuer("auth0").
        set_type("JWT").
        set_id("yutovo-server").
        set_issued_at(std::chrono::system_clock::now()).
        set_expires_at(std::chrono::system_clock::from_time_t(access_expires.secondsSinceEpoch())).
        set_payload_claim("access-uuid", jwt::claim(std::string(access_uuid))).
        set_payload_claim("login", jwt::claim(login)).
        sign(jwt::algorithm::rs256(public_key, private_key, "", ""));

    auto refresh_token = jwt::create().
        set_issuer("auth0").
        set_type("JWT").
        set_id("yutovo-server").
        set_issued_at(std::chrono::system_clock::now()).
        set_expires_at(std::chrono::system_clock::from_time_t(refresh_expires.secondsSinceEpoch())).
        set_payload_claim("refresh_uuid", jwt::claim(std::string(refresh_uuid))).
        set_payload_claim("login", jwt::claim(login)).
        sign(jwt::algorithm::rs256(public_key, private_key, "", ""));

    Json::Value r;
    r["document_id"] = document_id;
    r["name"] = name;
    HttpResponsePtr resp = HttpResponse::newHttpJsonResponse(r);
    resp->setStatusCode(k200OK);

    drogon::Cookie refresh_cookie("refresh_token", refresh_token);
    refresh_cookie.setHttpOnly(false);
    refresh_cookie.setPath("/");
    refresh_cookie.setExpiresDate(refresh_expires);
    resp->addCookie(refresh_cookie);

    if (!session_id.empty())
    {
        drogon::Cookie session_cookie("session_id", session_id);
        session_cookie.setHttpOnly(true);
        session_cookie.setPath("/");
        session_cookie.setExpiresDate(session_expires);
        resp->addCookie(session_cookie);
    }

    // if (!document_id.empty())
    //     SetDocumentCookie(document_id, resp);

    resp->addHeader("access_token", access_token);
    callback(resp);
}

void ControllerBase::SendJson(std::function<void (const HttpResponsePtr &)>& callback, const Json::Value& json)
{
    auto resp = HttpResponse::newHttpJsonResponse(json);
    resp->setStatusCode(k200OK);
    callback(resp);
}

void ControllerBase::SendJson(std::function<void (const HttpResponsePtr &)>& callback, const std::string& json)
{
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(json.c_str(), root))
    {
        SendError(k500InternalServerError, "Solver response error", callback);
        return;
    }
    SendJson(callback, root);
}

void ControllerBase::SendFile(std::function<void (const HttpResponsePtr &)>& callback, const fs::path& path)
{
    auto resp = HttpResponse::newFileResponse(path.c_str());
    resp->setStatusCode(k200OK);
    callback(resp);
}

void ControllerBase::SendError(const HttpStatusCode status_code, const char* description, std::function<void (const HttpResponsePtr &)>& callback)
{
    Json::Value r;
    r["error"] = description;
    auto resp = HttpResponse::newHttpJsonResponse(r);
    resp->setStatusCode(status_code);
    callback(resp);
    logger->Error("{}: {}", status_code, description);
}

bool ControllerBase::ParseRefreshToken(const std::string& refresh_token, std::string& refresh_uuid, std::string& login, 
    std::function<void (const HttpResponsePtr &)>& callback)
{
    try
    {
        auto decoded = jwt::decode(refresh_token);
        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::rs256(public_key, private_key, "", "")).with_issuer("auth0");
        verifier.verify(decoded);
        refresh_uuid = decoded.get_payload_claim("refresh_uuid").to_json().to_str();
        login = decoded.get_payload_claim("login").to_json().to_str();
    }
    catch (std::invalid_argument& ex)
    {
        SendError(k400BadRequest, ex.what(), callback);
        return false;
    }
    catch (jwt::error::claim_not_present_exception& ex)
    {
        SendError(k400BadRequest, ex.what(), callback);
        return false;
    }
    catch (jwt::token_verification_exception& ex)
    {
        SendError(k401Unauthorized, ex.what(), callback);
        return false;
    }

    return true;
}

bool ControllerBase::ParseId(const std::string& id_str, std::vector<int>& id)
{
    std::stringstream s(id_str);
    while (s.good())
    {
        std::string substr;
        getline(s, substr, ',');
        try
        {
            id.push_back(std::stoi(substr));
        }
        catch (std::exception const& ex)
        {
            return false;
        }
    }
    return true;
}

void ControllerBase::SetSessionCookie(const std::string& session_id, HttpResponsePtr resp)
{
    drogon::Cookie session_cookie("session_id", session_id);
    session_cookie.setHttpOnly(false);
    session_cookie.setPath("/");
    session_cookie.setExpiresDate(trantor::Date::now().after(session_expires));
    resp->addCookie(session_cookie);
}

void ControllerBase::SetDocumentCookie(const std::string& document_id, HttpResponsePtr resp)
{
    logger->Info("SetDocumentCookie document_id={}", document_id);
    drogon::Cookie document_cookie("document_id", document_id);
    document_cookie.setHttpOnly(false);
    document_cookie.setPath("/");
    document_cookie.setExpiresDate(trantor::Date::now().after(session_expires));
    resp->addCookie(document_cookie);
}

bool ControllerBase::AddSession(const std::string& document_id, std::string& session_id)
{
    session_id = std::string(boost::uuids::to_string(boost::uuids::random_generator()()));
    logger->Info("AddSession document_id={}, session_id={}", document_id, session_id);
    orm::DbClientPtr db = app().getDbClient();
    trantor::Date session_expires_date = trantor::Date::now().after(session_expires);
    auto result = db->execSqlSync("insert into user_sessions (session_id, expire_time, document_id) values ($1, $2, $3)", session_id, 
        session_expires_date.secondsSinceEpoch(), document_id);
    if (result.affectedRows() == 0)
        return false;
    return true;
}

bool ControllerBase::AddDocument(const std::string& user_id, std::string& document_id, std::string& name)
{
    if (name.empty())
    {
        //create a unique name
        int num = 0;
        orm::DbClientPtr db = app().getDbClient();
        orm::Result result = db->execSqlSync("select name from user_documents where user_id=$1 and (lower(name) LIKE 'document_%')", user_id);
        for (int i = 0; i < result.size(); ++i)
        {
            auto row = result[i];
            std::string n = row["name"].as<std::string>();
            n = n.substr(9);
            try
            {
                int num_ = std::stoi(n);
                if (num_ > num)
                    num = num_;
            }
            catch (std::exception& ex)
            {
            }
        }
        name = "document_" + std::to_string(num + 1);
    }

    orm::DbClientPtr db = app().getDbClient();
    orm::Result result = db->execSqlSync("insert into user_documents (user_id, name, document) values ($1, $2, $3) returning document_id", 
        user_id, name, empty_document);
    if (result.affectedRows() == 0)
        return false;

    auto row = result[0];
    document_id = row["document_id"].as<std::string>();
    logger->Info("Document added document_id={}, name={}", document_id, name);
    return true;
}

int ControllerBase::GetFirstEmptyDocument(const std::string& user_id)
{
    Json::Value doc;
    Json::Reader reader;
    orm::DbClientPtr db = app().getDbClient();
    orm::Result result = db->execSqlSync("select document_id, document from user_documents where user_id=$1", user_id);
    for (int i = 0; i < result.size(); ++i)
    {
        auto row = result[i];
        auto d = row["document"].as<std::string>();
        if (!reader.parse(d, doc))
            continue;
        if (!doc.isObject() || !doc.isMember("text"))
            continue;
        Json::Value& text = doc["text"];
        try
        {
            auto& el = text["elements"][0]["elements"][0]["elements"][0];
            if (el["elements"] == "")
                return (int)row["document_id"].as<int>();
        }
        catch (Json::Exception& ex)
        {
            continue;
        }
    }
    return -1;
}

void ControllerBase::LogJson(const Json::Value& value)
{
    Json::FastWriter fastWriter;
    std::string output = fastWriter.write(value);
    logger->Info("{}", output);
}
}
