#include "controller_base.h"
#include <jwt-cpp/jwt.h>
#include <fstream>
#include <system_error>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace yutovo_server
{

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
    session_expires = v.get("session_expire_timeout", 3600).asInt();
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
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_PLAIN);
    SetDocumentCookie(document_id, resp);
    callback(resp);
}

void ControllerBase::SendOkTokens(std::function<void (const HttpResponsePtr &)>& callback, const std::string& login, const std::string& access_uuid, 
    const std::string& refresh_uuid, const std::string& user_session, trantor::Date access_expires, trantor::Date refresh_expires, 
    trantor::Date session_expires)
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

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_PLAIN);

    drogon::Cookie refresh_cookie("refresh_token", refresh_token);
    refresh_cookie.setHttpOnly(false);
    refresh_cookie.setPath("/");
    refresh_cookie.setExpiresDate(refresh_expires);
    resp->addCookie(refresh_cookie);

    if (!user_session.empty())
    {
        drogon::Cookie session_cookie("user_session", user_session);
        session_cookie.setHttpOnly(true);
        session_cookie.setPath("/");
        session_cookie.setExpiresDate(session_expires);
        resp->addCookie(session_cookie);
    }

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

void ControllerBase::SetDocumentCookie(const std::string& document_id, HttpResponsePtr resp)
{
    drogon::Cookie document_cookie("document_id", document_id);
    document_cookie.setHttpOnly(true);
    document_cookie.setPath("/");
    document_cookie.setExpiresDate(trantor::Date::now().after(session_expires));
    resp->addCookie(document_cookie);
}
}
