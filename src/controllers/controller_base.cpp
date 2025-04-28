#include "controller_base.h"
#include <jwt-cpp/jwt.h>
#include <fstream>
#include <system_error>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <openssl/md5.h>

namespace yutovo_server
{

//LoginFilter

LoginFilter::LoginFilter()
{
    std::ifstream key_file("yutovo_service.key");
    if (!key_file.is_open())
        throw std::system_error(ENOENT, std::generic_category(), "Key file not open");
    
    std::stringstream ss;
    ss << key_file.rdbuf();
    private_key = ss.str();
}

void LoginFilter::doFilter(const HttpRequestPtr& req, FilterCallback&& not_valid_callback, FilterChainCallback&& valid_callback)
{
    std::string session_id = req->getCookie("session_id");
    if (!session_id.empty() && !IsGuid(session_id))
    {
        GetLogger(session_id)->Error("LoginFilter SessionId error: {}", session_id);
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k403Forbidden);
        not_valid_callback(resp);
        return;
    }

    SessionPtr session = req->session();
    if (session_id.empty())
        session_id = session->get<std::string>("session_id");
    GetLogger(session_id)->Debug("doFilter {}", req->path());
    std::string access_token = req->getHeader("access_token");
    HttpResponsePtr resp;

    try
    {
        auto decoded = jwt::decode(access_token);
        auto login = decoded.get_payload_claim("login").to_json().to_str();
        if (login != session->get<std::string>("login"))
        {
            GetLogger(session_id)->Error("Unauthorized: {}", login);
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
        GetLogger(session_id)->Error("Wrong access token: {}", ex.what());
        resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
    }
    catch (jwt::error::claim_not_present_exception& ex)
    {
        GetLogger(session_id)->Error("Wrong access token: {}", ex.what());
        resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
    }
    catch (jwt::token_verification_exception& ex)
    {
        GetLogger(session_id)->Error("Wrong access token: {}", ex.what());
        resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k403Forbidden);
    }
    catch (...)
    {
        GetLogger(session_id)->Error("Wrong access token");
        resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k405MethodNotAllowed);
    }

    not_valid_callback(resp);
}

//ControllerBase

ControllerBase::ControllerBase()
{
    std::ifstream key_file("yutovo_service.key");
    if (!key_file.is_open())
        throw std::system_error(ENOENT, std::generic_category(), "Pirvate key file not open");
    
    std::stringstream ss;
    ss << key_file.rdbuf();
    private_key = ss.str();

    std::ifstream public_key_file("yutovo_service.pub");
    if (!public_key_file.is_open())
        throw std::system_error(ENOENT, std::generic_category(), "Public key file not open");
    
    ss.str("");
    ss << public_key_file.rdbuf();
    public_key = ss.str();

    const Json::Value& v = app().getCustomConfig();
    session_expires = v.get("session_expire_timeout", 86400).asInt();

    library_path = v.get("library_path", "").asString();
    if (library_path.empty())
        throw std::system_error(ENOTDIR, std::generic_category(), "Library path not defined");
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
    LogJson(GetLogger(session_id), r);
    callback(resp);
}

void ControllerBase::SendOk(std::function<void (const HttpResponsePtr &)>& callback, const std::string& document_id, const std::string& name)
{
    Json::Value r;
    r["document_id"] = document_id;
    r["name"] = name;
    auto resp = HttpResponse::newHttpJsonResponse(r);
    resp->setStatusCode(k200OK);
    LogJson(GetLogger(session_id), r);
    callback(resp);
}

void ControllerBase::SendOkTokens(std::function<void (const HttpResponsePtr &)>& callback, const std::string& login, const std::string& access_uuid, 
    const std::string& refresh_uuid, const std::string& session_id, trantor::Date access_expires, trantor::Date refresh_expires, 
    trantor::Date session_expires, const std::string document_id, const std::string name, const std::string language, const std::string settings)
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
    if (document_id != "-1")
        r["document_id"] = document_id;
    r["name"] = name;
    r["language"] = language;
    r["settings"] = settings;
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
        SendError(k500InternalServerError, "Json error", callback);
        return;
    }
    SendJson(callback, root);
}

void ControllerBase::SendFile(std::function<void (const HttpResponsePtr &)>& callback, const fs::path& path)
{
    auto resp = HttpResponse::newFileResponse(path.c_str());
    callback(resp);
}

void ControllerBase::SendCaptcha(std::function<void (const HttpResponsePtr &)>& callback, const cimg_library::CImg<unsigned char>& image)
{
    unsigned buf_size = 1024 * 1024;
    std::unique_ptr<JOCTET> buffer(new JOCTET[buf_size]);
    image.save_jpeg_buffer(buffer.get(), buf_size, 60);

    //convert the picture to base64
    std::string image_base64;
    int val = 0, valb = -6;
    unsigned char* buf = buffer.get();
    for (size_t i = 0; i < buf_size; ++i)
    {
        unsigned char c = buf[i];
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0)
        {
            image_base64.push_back(base[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6)
        image_base64.push_back(base[((val << 8) >> (valb + 8)) & 0x3F]);
    while (image_base64.size() % 4)
        image_base64.push_back('=');

    Json::Value r;
    r["captcha"] = image_base64;
    auto resp = HttpResponse::newHttpJsonResponse(r);
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
    GetLogger(session_id)->Error("{}: {}", status_code, description);
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
    GetLogger(session_id)->Info("SetDocumentCookie document_id={}", document_id);
    drogon::Cookie document_cookie("document_id", document_id);
    document_cookie.setHttpOnly(false);
    document_cookie.setPath("/");
    document_cookie.setExpiresDate(trantor::Date::now().after(session_expires));
    resp->addCookie(document_cookie);
}

bool ControllerBase::AddSession(const std::string& document_id)
{
    session_id = std::string(boost::uuids::to_string(boost::uuids::random_generator()()));
    GetLogger("")->Info("AddSession document_id={}, session_id={}", document_id, session_id);

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
    db->execSqlSync("update users set document_id=$1 where user_id=$2", document_id, user_id);
    GetLogger(session_id)->Info("Document added document_id={}, name={}", document_id, name);
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
            auto s = el.toStyledString();
            if (el["elements"] == "" || (el["elements"].size() == 1 && el["elements"][0]["elements"].size() == 1 && 
                el["elements"][0]["elements"][0]["elements"].size() == 1 && el["elements"][0]["elements"][0]["elements"][0]["elements"] == ""))
            {
                return (int)row["document_id"].as<int>();
            }
        }
        catch (Json::Exception& ex)
        {
            continue;
        }
    }
    return -1;
}

std::string ControllerBase::GetHash(const std::string& str, const std::string& salt)
{
    unsigned char hash[MD5_DIGEST_LENGTH];
    std::string s = str + salt;
    MD5((const unsigned char*)s.c_str(), s.size(), hash);
    char hash_str[MD5_DIGEST_LENGTH * 2];
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf(&hash_str[i * 2], "%02x", (unsigned int)hash[i]);
    return std::string(&hash_str[0], MD5_DIGEST_LENGTH * 2);
}

bool ControllerBase::GetSessionId(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>& callback)
{
    session_id = req->getCookie("session_id");
    if (session_id.empty())
    {
        SessionPtr session = req->session();
        session_id = session->get<std::string>("session_id");
        return true;
    }

    //check the SessionId is valid
    if (!IsGuid(session_id))
    {
        GetLogger("")->Error("SessionId error: {}", session_id);
        SendError(k403Forbidden, "SessionId error", callback);
        return false;
    }
    return true;
}

void ControllerBase::LogJson(yutovo::Logger* logger, const Json::Value& value)
{
    Json::FastWriter fastWriter;
    std::string output = fastWriter.write(value);
    if (!output.empty() && output[output.size() - 1] == '\n')
        output.pop_back();
    logger->Info("{}", output);
}

}
