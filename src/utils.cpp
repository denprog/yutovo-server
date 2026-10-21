/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "utils.h"
#include <regex>

namespace yutovo_server
{

extern std::string GetDeployPath();

yutovo::Logger* logger = yutovo::Logger::GetInstance(GetDeployPath() + "/log/yutovo-server", "server", true, true);

yutovo::Logger* GetLogger(const std::string& session_id)
{
    if (session_id.empty() || !IsGuid(session_id))
        return logger;
    return yutovo::Logger::GetInstance(GetDeployPath() + "/log/yutovo-server/sessions/" + session_id, "server", true, true);
}

yutovo::Logger* GetSolverLogger(const std::string& solver_id)
{
    if (!IsGuid(solver_id))
        return logger;
    return yutovo::Logger::GetInstance(GetDeployPath() + "/log/yutovo-server/solver/" + solver_id, "solver", true, true);
}

yutovo::Logger* GetCalculatorLogger(const std::string& solver_id)
{
    if (!IsGuid(solver_id))
        return logger;
    return yutovo::Logger::GetInstance(GetDeployPath() + "/log/yutovo-server/calculator/" + solver_id, "calculator", true, true);
}

yutovo::Logger* GetUpdatesLogger(const std::string& session_id)
{
    if (!IsGuid(session_id))
        return logger;
    return yutovo::Logger::GetInstance(GetDeployPath() + "/log/yutovo-server/updates/" + session_id, "calculator", true, true);
}

bool IsGuid(const std::string& str)
{
    static std::regex guid("(^([0-9A-Fa-f]{8}[-]?[0-9A-Fa-f]{4}[-]?[0-9A-Fa-f]{4}[-]?[0-9A-Fa-f]{4}[-]?[0-9A-Fa-f]{12})$)");
    return std::regex_match(str, guid);
}

static std::string Base64EncodeString(const std::string& data)
{
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string res;
    res.reserve((data.size() + 2) / 3 * 4);
    for (size_t i = 0; i < data.size(); i += 3)
    {
        unsigned int chunk = (unsigned char)data[i] << 16;
        if (i + 1 < data.size())
            chunk |= (unsigned char)data[i + 1] << 8;
        if (i + 2 < data.size())
            chunk |= (unsigned char)data[i + 2];
        res += table[(chunk >> 18) & 0x3F];
        res += table[(chunk >> 12) & 0x3F];
        res += (i + 1 < data.size()) ? table[(chunk >> 6) & 0x3F] : '=';
        res += (i + 2 < data.size()) ? table[chunk & 0x3F] : '=';
    }
    return res;
}

bool MailUtils::IsEmail(const std::string& str)
{
    if (str.empty() || str.size() > 254)
        return false;
    static std::regex email("^([A-Za-z0-9._%+-]{1,64})@([A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?\\.)+[A-Za-z]{2,63}$");
    return std::regex_match(str, email);
}

std::string MailUtils::StripHeaderNewlines(const std::string& value)
{
    std::string res;
    res.reserve(value.size());
    for (char c : value)
    {
        if (c != '\r' && c != '\n' && c != '\0')
            res += c;
    }
    return res;
}

std::string MailUtils::EncodeSubject(const std::string& subject)
{
    std::string value = StripHeaderNewlines(subject);
    if (value.size() > 255)
    {
        //do not cut a multi-byte utf-8 character in half
        size_t end = 255;
        while (end > 0 && ((unsigned char)value[end] & 0xC0) == 0x80)
            --end;
        value.resize(end);
    }

    bool ascii = true;
    for (char c : value)
    {
        if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7E)
        {
            ascii = false;
            break;
        }
    }
    if (ascii)
        return value;

    //RFC 2047 encoded-words, at most 45 bytes of payload to fit the 75-character limit
    std::string encoded;
    size_t i = 0;
    while (i < value.size())
    {
        size_t len = value.size() - i < 45 ? value.size() - i : 45;
        while (len > 0 && i + len < value.size() && ((unsigned char)value[i + len] & 0xC0) == 0x80)
            --len;
        if (len == 0)
            len = 1;
        if (i > 0)
            encoded += "\r\n ";
        encoded += "=?UTF-8?B?" + Base64EncodeString(value.substr(i, len)) + "?=";
        i += len;
    }
    return encoded;
}

std::string MailUtils::QuotedPrintableEncode(const std::string& value)
{
    static const char hex[] = "0123456789ABCDEF";
    const size_t max_line = 72;
    std::string res;
    std::string line;

    //a space or tab right before a line break must be encoded
    auto encode_trailing_space = [&line]()
    {
        if (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
        {
            unsigned char b = line.back();
            line.pop_back();
            line += '=';
            line += hex[b >> 4];
            line += hex[b & 0x0F];
        }
    };

    for (size_t i = 0; i < value.size(); ++i)
    {
        unsigned char b = value[i];
        std::string token;

        if (b == '\r')
        {
            if (i + 1 < value.size() && value[i + 1] == '\n')
            {
                ++i;
                encode_trailing_space();
                res += line + "\r\n";
                line.clear();
                continue;
            }
            token = "=0D";
        }
        else if (b == '\n')
            token = "=0A";
        else if (b == '=')
            token = "=3D";
        else if (b == '.' && line.empty())
            token = "=2E";
        else if (b == '\t' || b == ' ' || (b >= 0x21 && b <= 0x7E))
            token += (char)b;
        else
        {
            token += '=';
            token += hex[b >> 4];
            token += hex[b & 0x0F];
        }

        if (line.size() + token.size() > max_line)
        {
            //soft line break
            encode_trailing_space();
            res += line + "=\r\n";
            line.clear();
        }
        line += token;
    }

    encode_trailing_space();
    res += line;
    return res;
}

MailUtils::Verdict MailUtils::Check(const std::string& stored, const std::string& submitted, int failed_attempts, int64_t age_seconds)
{
    if (stored.empty() || submitted.empty() || age_seconds < 0 || age_seconds > ttl_seconds)
        return Verdict::kInvalidated;
    if (stored == submitted)
        return Verdict::kAccepted;
    if (failed_attempts + 1 >= max_attempts)
        return Verdict::kInvalidated;
    return Verdict::kRejected;
}

std::string GetDeployPath()
{
    char* p = std::getenv("YUTOVO_DEPLOY");
    return p ? p : ".";
}

}
