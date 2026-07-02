/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __BASE_CONTROLLER_H__
#define __BASE_CONTROLLER_H__

#include <drogon/HttpController.h>
#include <filesystem>
#include <curl/curl.h>
#include <vector>
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
    void SendOk(std::function<void (const HttpResponsePtr &)>& callback, const std::string& document_id, const std::string& session_id);
    void SendOkDocumentName(std::function<void (const HttpResponsePtr &)>& callback, const std::string& document_id, const std::string& session_id, 
        const std::string& name);
    void SendOkTokens(std::function<void (const HttpResponsePtr &)>& callback, const std::string& login, const std::string& access_uuid, 
        const std::string& refresh_uuid, const std::string& session_id, trantor::Date access_expires, trantor::Date refresh_expires, 
        trantor::Date session_expires, const std::string document_id = "", const std::string name = "", const std::string language = "", 
        const std::string settings = "");
    
    void SendJson(std::function<void (const HttpResponsePtr &)>& callback, const Json::Value& json);
    void SendJson(std::function<void (const HttpResponsePtr &)>& callback, const std::string& json, const std::string& session_id);
    void SendFile(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>& callback, const fs::path& path);
    void SendCaptcha(std::function<void (const HttpResponsePtr &)>& callback, const cimg_library::CImg<unsigned char>& image);
    void SendError(const HttpStatusCode status_code, const char* description, const std::string& session_id, 
        std::function<void (const HttpResponsePtr &)>& callback);

    bool ParseRefreshToken(const std::string& refresh_token, std::string& refresh_uuid, std::string& login, const std::string& session_id, 
        std::function<void (const HttpResponsePtr &)>& callback);
    
    bool ParseId(const std::string& id_str, std::vector<int>& id);

    void SetSessionCookie(const std::string& session_id, HttpResponsePtr resp);
    void SetDocumentCookie(const std::string& document_id, const std::string& session_id, HttpResponsePtr resp);

    bool AddSession(const std::string& document_id, std::string& session_id);
    bool AddDocument(const HttpRequestPtr& req, const std::string& user_id, std::string& document_id, std::string& name, int language, 
        std::function<void (const HttpResponsePtr &)>& callback);

    int GetFirstEmptyDocument(const std::string& user_id);

    std::string HashPassword(const std::string& password);
    bool VerifyPassword(const std::string& password, const std::string& stored_hash, std::string* new_hash = nullptr);

    bool GetSessionId(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>& callback, std::string& session_id);

    void LogJson(yutovo::Logger* logger, const Json::Value& value);

    std::string JsonToString(Json::Value& value);

protected:
    struct EmailAttachment
    {
        std::string filename;
        std::string content_type;
        std::string data;
    };

    struct EmailReadContext
    {
        std::string* message;
        size_t offset;
    };

    static std::string HtmlEscape(const std::string& value);
    static size_t EmailPayload(char* ptr, size_t size, size_t nmemb, void* userp);
    static std::string WrapBase64(const std::string& base64);

    CURLcode SendEmail(const std::string& from, const std::string& to, const std::string& subject, const std::string& message_html,
        const std::vector<EmailAttachment>& attachments = {});

    int session_expires = 0; //session without user, in seconds, after last using
    std::string public_key, private_key;
    int access_token_expires = 60 * 2; //seconds
    int refresh_token_expires = 30 * 60 * 60 * 24; //seconds

    inline static const std::string empty_document = "{\"text\": {\"id\": \"0\", \"type\": 1, \"elements\": [{\"id\": \"0,0\", \"type\": 2,\
        \"elements\": [{\"id\": \"0,0,0\", \"type\": 3, \"elements\": [{\"id\": \"0,0,0,0\", \"type\": 5, \"elements\": [{\"id\": \"0,0,0,0,0\",\
        \"type\": 6, \"elements\": [{\"id\": \"0,0,0,0,0,0\", \"type\": 7, \"elements\": [{\"id\": \"0,0,0,0,0,0,0\", \"type\": 8,\
        \"elements\": \"\"}]}], \"format_name\": \"Code\", \"format_alignment\": 0}], \"code_id\": 1}]}]}]}, \"caret\": {\"id\": \"0,0,0,0,0,0,0\"},\
        \"selection\": []}";

    std::string library_path;
    inline static const std::string base = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

private:
    static constexpr int pbkdf_iterations = 100000;
    static constexpr int salt_bytes = 16;
    static constexpr int hash_bytes = 32;

    static std::string Base64Encode(const unsigned char* data, size_t len);
    static std::vector<unsigned char> Base64Decode(const std::string& in);
    static std::string BytesToHex(const unsigned char* data, size_t len);
    static bool IsHexString(const std::string& s);
    static std::string GetMd5Hash(const std::string& str, const std::string& salt);
};

}

#endif
