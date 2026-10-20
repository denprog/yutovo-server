/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <gtest/gtest.h>
#include "mock.h"
#include "../src/utils.h"
#include "../src/email_templates.h"

namespace yutovo_server_test
{

using namespace drogon;

TEST(MailUtilsTest, IsEmailValid)
{
    EXPECT_TRUE(yutovo_server::MailUtils::IsEmail("user@example.com"));
    EXPECT_TRUE(yutovo_server::MailUtils::IsEmail("first.last+tag@sub.domain.io"));
    EXPECT_TRUE(yutovo_server::MailUtils::IsEmail("a@b.ru"));
    EXPECT_TRUE(yutovo_server::MailUtils::IsEmail("no-reply@yutovo.ru"));
}

TEST(MailUtilsTest, IsEmailInvalid)
{
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail(""));
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail("no-at-sign.com"));
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail("user@example"));
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail("a@b.c"));
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail("user @example.com"));
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail("@example.com"));
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail("user@-bad-.com"));
    //header and SMTP command injection payloads
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail("a@b.com\r\nBcc: victim@example.com"));
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail("a@b.com\nRCPT TO:<c@d.com>"));
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail("a@b.com "));
    //length limits
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail(std::string(65, 'a') + "@x.com"));
    EXPECT_FALSE(yutovo_server::MailUtils::IsEmail(std::string(255, 'a') + "@x.com"));
}

TEST(MailUtilsTest, StripHeaderNewlines)
{
    EXPECT_EQ(yutovo_server::MailUtils::StripHeaderNewlines("plain value"), "plain value");
    EXPECT_EQ(yutovo_server::MailUtils::StripHeaderNewlines("injected\r\nBcc: x@y.z"), "injectedBcc: x@y.z");
    EXPECT_EQ(yutovo_server::MailUtils::StripHeaderNewlines("a\r\nb\nc\rd"), "abcd");
    std::string with_nul = std::string("ab") + '\0' + "cd";
    EXPECT_EQ(yutovo_server::MailUtils::StripHeaderNewlines(with_nul), "abcd");
}

TEST(MailUtilsTest, EncodeSubjectAscii)
{
    EXPECT_EQ(yutovo_server::MailUtils::EncodeSubject("Simple subject"), "Simple subject");
    std::string encoded = yutovo_server::MailUtils::EncodeSubject("Ok\r\nBcc: victim@example.com");
    EXPECT_EQ(encoded, "OkBcc: victim@example.com");
    EXPECT_EQ(encoded.find('\r'), std::string::npos);
    EXPECT_EQ(encoded.find('\n'), std::string::npos);
}

TEST(MailUtilsTest, EncodeSubjectNonAscii)
{
    std::string encoded = yutovo_server::MailUtils::EncodeSubject("Смена пароля");
    EXPECT_NE(encoded.find("=?UTF-8?B?"), std::string::npos);
    EXPECT_NE(encoded.find("?="), std::string::npos);
    //only ASCII characters can remain in the encoded header
    for (char c : encoded)
        EXPECT_TRUE((unsigned char)c < 0x80);

    //a long non-ASCII subject is folded into several encoded-words
    std::string long_subject;
    for (int i = 0; i < 60; ++i)
        long_subject += "тема письма ";
    encoded = yutovo_server::MailUtils::EncodeSubject(long_subject);
    EXPECT_NE(encoded.find("=?UTF-8?B?"), std::string::npos);
    size_t start = 0;
    while (start < encoded.size())
    {
        size_t end = encoded.find("\r\n", start);
        size_t len = (end == std::string::npos ? encoded.size() : end) - start;
        EXPECT_TRUE(len <= 76) << len;
        start = (end == std::string::npos) ? encoded.size() : end + 2;
    }
}

TEST(MailUtilsTest, QuotedPrintableEncoding)
{
    EXPECT_EQ(yutovo_server::MailUtils::QuotedPrintableEncode("plain"), "plain");
    EXPECT_EQ(yutovo_server::MailUtils::QuotedPrintableEncode("a=b"), "a=3Db");
    EXPECT_EQ(yutovo_server::MailUtils::QuotedPrintableEncode(".dot"), "=2Edot");
    EXPECT_EQ(yutovo_server::MailUtils::QuotedPrintableEncode("a.dot"), "a.dot");
    //existing line breaks stay hard breaks
    EXPECT_EQ(yutovo_server::MailUtils::QuotedPrintableEncode("line1\r\nline2"), "line1\r\nline2");
    //a lone LF must not create a break
    EXPECT_EQ(yutovo_server::MailUtils::QuotedPrintableEncode("a\nb"), "a=0Ab");
    //trailing spaces must be encoded
    EXPECT_EQ(yutovo_server::MailUtils::QuotedPrintableEncode("a "), "a=20");
    EXPECT_EQ(yutovo_server::MailUtils::QuotedPrintableEncode("a \r\nb"), "a=20\r\nb");
    //high bytes are encoded: "Я" in utf-8
    EXPECT_EQ(yutovo_server::MailUtils::QuotedPrintableEncode("\xD0\xAF"), "=D0=AF");
}

TEST(MailUtilsTest, QuotedPrintableSoftBreaks)
{
    std::string long_line(200, 'x');
    std::string encoded = yutovo_server::MailUtils::QuotedPrintableEncode(long_line);
    size_t start = 0;
    while (start < encoded.size())
    {
        size_t end = encoded.find("\r\n", start);
        size_t len = (end == std::string::npos ? encoded.size() : end) - start;
        EXPECT_TRUE(len <= 76) << len;
        start = (end == std::string::npos) ? encoded.size() : end + 2;
    }
    //removing the soft breaks restores the original line
    std::string unfolded = encoded;
    size_t p;
    while ((p = unfolded.find("=\r\n")) != std::string::npos)
        unfolded.erase(p, 3);
    EXPECT_EQ(unfolded, long_line);
}

TEST(EmailTemplatesTest, VerificationTemplates)
{
    yutovo_server::EmailTemplate t = yutovo_server::EmailTemplates::GetVerificationEmailTemplate("register", "ru");
    EXPECT_EQ(t.subject, "Регистрация на Ютово");
    EXPECT_NE(t.body_html.find("EMAIL_CODE"), std::string::npos);
    EXPECT_NE(t.body_html.find("Подтвердите свою регистрацию"), std::string::npos);
    EXPECT_EQ(t.body_html.find("{{"), std::string::npos);

    t = yutovo_server::EmailTemplates::GetVerificationEmailTemplate("recover_password", "ru");
    EXPECT_EQ(t.subject, "Смена пароля");
    t = yutovo_server::EmailTemplates::GetVerificationEmailTemplate("change_password", "ru");
    EXPECT_EQ(t.subject, "Смена пароля");
    t = yutovo_server::EmailTemplates::GetVerificationEmailTemplate("change_email", "en");
    EXPECT_EQ(t.subject, "Changing e-mail");
    t = yutovo_server::EmailTemplates::GetVerificationEmailTemplate("register", "de");
    EXPECT_EQ(t.subject, "Registrierung bei Yutovo");

    //unknown values fall back to the registration template in english
    t = yutovo_server::EmailTemplates::GetVerificationEmailTemplate("register", "xx");
    EXPECT_EQ(t.subject, "Register code");
    t = yutovo_server::EmailTemplates::GetVerificationEmailTemplate("unknown", "fr");
    EXPECT_EQ(t.subject, "Inscription sur Yutovo");
}

//The handler must reject invalid recipients before any SMTP activity
TEST_F(AuthTest, send_email_code_wrong_email)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["email"] = "a@b.com\r\nBcc: victim@example.com";
    body["captcha"] = "0";
    body["action"] = "register";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/send-email-code");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    EXPECT_TRUE(r->getStatusCode() == k400BadRequest) << r->getStatusCode();
}

TEST_F(AuthTest, send_email_code_not_an_email)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    StartPage(client);

    Json::Value body;
    body["email"] = "not-an-email";
    body["login"] = "User1";
    body["captcha"] = "0";
    auto req = HttpRequest::newHttpJsonRequest(body);
    req->setMethod(drogon::Post);
    req->setPath("/auth/send-email-code");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    EXPECT_TRUE(r->getStatusCode() == k400BadRequest) << r->getStatusCode();
}

}
