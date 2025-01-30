#ifndef __BASE_CONTROLLER_H__
#define __BASE_CONTROLLER_H__

#include <drogon/HttpController.h>
#include <filesystem>
#include <jpeglib.h>
#include <jerror.h>
#define cimg_plugin "plugins/jpeg_buffer.h"
#include <CImg.h>
#include "utils.h"

using namespace drogon;
using namespace yutovo;

namespace yutovo_server
{

namespace fs = std::filesystem;

class LoginFilter : public drogon::HttpFilter<LoginFilter>
{
public:
    LoginFilter();

    virtual void doFilter(const HttpRequestPtr& req, FilterCallback&& not_valid_callback, FilterChainCallback&& valid_callback) override;

private:
    std::string public_key, private_key;
};

class ControllerBase
{
public:
    ControllerBase();
    
protected:
    void SendOk(std::function<void (const HttpResponsePtr &)>& callback);
    void SendOk(std::function<void (const HttpResponsePtr &)>& callback, const std::string& document_id);
    void SendOk(std::function<void (const HttpResponsePtr &)>& callback, const std::string& document_id, const std::string& name);

    void SendOkTokens(std::function<void (const HttpResponsePtr &)>& callback, const std::string& login, const std::string& access_uuid, 
        const std::string& refresh_uuid, const std::string& session_id, trantor::Date access_expires, trantor::Date refresh_expires, 
        trantor::Date session_expires, const std::string document_id = "", const std::string name = "", const std::string language = "", 
        const std::string settings = "");
    
    void SendJson(std::function<void (const HttpResponsePtr &)>& callback, const Json::Value& json);
    void SendJson(std::function<void (const HttpResponsePtr &)>& callback, const std::string& json);
    void SendFile(std::function<void (const HttpResponsePtr &)>& callback, const fs::path& path);
    void SendCaptcha(std::function<void (const HttpResponsePtr &)>& callback, const cimg_library::CImg<unsigned char>& image);
    void SendError(const HttpStatusCode status_code, const char* description, std::function<void (const HttpResponsePtr &)>& callback);

    bool ParseRefreshToken(const std::string& refresh_token, std::string& refresh_uuid, std::string& login, 
        std::function<void (const HttpResponsePtr &)>& callback);
    
    bool ParseId(const std::string& id_str, std::vector<int>& id);

    void SetSessionCookie(const std::string& session_id, HttpResponsePtr resp);
    void SetDocumentCookie(const std::string& document_id, HttpResponsePtr resp);

    bool AddSession(const std::string& document_id);
    bool AddDocument(const std::string& user_id, std::string& document_id, std::string& name);

    int GetFirstEmptyDocument(const std::string& user_id);

private:
    void LogJson(const Json::Value& value);

protected:
    int session_expires = 0; //session without user, in seconds, after last using
    std::string public_key, private_key;
    int access_token_expires = 60 * 2; //seconds
    int refresh_token_expires = 60 * 60 * 24; //seconds

    inline static const std::string empty_document = "{\"text\": {\"id\": \"0\", \"type\": 1, \"elements\": [{\"id\": \"0,0\", \"type\": 2,\
        \"elements\": [{\"id\": \"0,0,0\", \"type\": 3, \"elements\": [{\"id\": \"0,0,0,0\", \"type\": 5, \"elements\": [{\"id\": \"0,0,0,0,0\",\
        \"type\": 6, \"elements\": [{\"id\": \"0,0,0,0,0,0\", \"type\": 7, \"elements\": [{\"id\": \"0,0,0,0,0,0,0\", \"type\": 8,\
        \"elements\": \"\"}]}], \"format_name\": \"Code\", \"format_alignment\": 0}], \"code_id\": 1}]}]}]}, \"caret\": {\"id\": \"0,0,0,0,0,0,0\"},\
        \"selection\": []}";

    std::string library_path;
    inline static const std::string base = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string session_id;
};

}

#endif
