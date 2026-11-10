/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#ifndef __SSR_HTML_H__
#define __SSR_HTML_H__

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <filesystem>
#include <json/json.h>

namespace yutovo_server
{

//Converts a .yut document json into SEO html
class SsrDocument
{
public:
    SsrDocument(const Json::Value& document);

    std::string GenerateArticle(const std::string& title) const;
    std::string FirstParagraphText(size_t max_length) const;
    int Language() const;

    static std::string EscapeHtml(const std::string& str);
    static int HeadingLevel(const std::string& format_name);
    static std::string LangFromLanguage(int language);
    static bool ReadLibraryFile(const std::string& path, Json::Value& document);

private:
    static constexpr unsigned max_element_depth = 200; //recursion guard of ElementToHtml

    struct DepthGuard
    {
        explicit DepthGuard(const SsrDocument* _document) : document(_document)
        {
            ++document->depth;
        }

        ~DepthGuard()
        {
            --document->depth;
        }

        const SsrDocument* document;
    };

    std::string ElementsToHtml(const Json::Value& element) const;
    std::string ElementToHtml(const Json::Value& element) const;
    std::string ParagraphsToHtml(const std::vector<const Json::Value*>& element_arrays) const;
    std::string ParagraphToHtml(const Json::Value& element) const;
    std::string ListItemToHtml(const Json::Value& element) const;
    std::string StringToHtml(const Json::Value& element) const;
    std::string LinkToHtml(const Json::Value& element) const;
    std::string CodeParagraphToHtml(const Json::Value& element) const;
    std::string CodeBlockToHtml(const Json::Value& element) const;
    std::string CodeParagraphsBlockToHtml(const Json::Value& element) const;
    std::string OperatorToHtml(const Json::Value& element, const std::string& default_symbol) const;
    std::string LeftRightToHtml(const Json::Value& element, const std::string& symbol) const;
    std::string ResultToHtml(const Json::Value& element) const;
    std::string IterationToHtml(const Json::Value& element, const std::string& symbol) const;
    std::string DefiniteIntegralToHtml(const Json::Value& element) const;
    std::string IndefiniteIntegralToHtml(const Json::Value& element) const;
    std::string LimitToHtml(const Json::Value& element) const;
    std::string EvaluationBarToHtml(const Json::Value& element) const;
    std::string MatrixToHtml(const Json::Value& element) const;
    std::string GraphToHtml(const Json::Value& element) const;
    std::string StringStyle(const Json::Value* format) const;
    const Json::Value* FindStringFormat(const Json::Value& element) const;
    static std::string ColorToHtml(unsigned int color);
    static unsigned int ColorFromJson(const Json::Value& value, unsigned int default_color);
    static std::string AlignmentAttributes(const Json::Value& element);
    static bool IsListMarker(const std::string& marker);
    static const Json::Value& Child(const Json::Value& element, unsigned index);

    const Json::Value& document;
    std::map<std::string, const Json::Value*> string_formats;
    mutable unsigned depth = 0;
};

//Injects the article into the SPA index.html page as <canvas> fallback content
class SsrPage
{
public:
    //The site hosts: the version for the Russian users and the default one
    static constexpr const char* russia_host = "yutovo.ru";
    static constexpr const char* default_host = "yutovo.com";

    //Returns the absolute url of the page on its own site host: the yutovo.ru requests stay on yutovo.ru, the rest use yutovo.com
    static std::string CanonicalUrl(const std::string& host, const std::string& path);

    //Returns the index.html page with the canonical and hreflang head links inserted, or an empty string when the head is not found
    static std::string AddHeadLinks(const std::string& index_html, const std::string& canonical_url);

    //Returns the page html, or an empty string when the app container is not found
    static std::string MakePage(const std::string& index_html, const std::string& title, const std::string& description, 
        const std::string& lang, const std::string& article, const std::string& canonical_url);

private:
    //Returns the canonical link of the page and the hreflang alternates of the two site hosts
    static std::string HeadLinks(const std::string& canonical_url);

    //Returns the url with the host part replaced
    static std::string UrlOnHost(const std::string& url, const char* host);

    //Returns the position in the page head where new tags are inserted, npos when the head is not found
    static size_t HeadInsertPosition(const std::string& page);
};

//Caches the index.html template and generated library pages, keyed by the canonical url, invalidated by mtime
class SsrCache
{
public:
    using FileTime = std::filesystem::file_time_type;

    static SsrCache& GetInstance();

    bool GetIndexPage(std::string& index_html);
    bool GetLibraryPage(const std::string& path, const std::string& title, const std::string& canonical_url, std::string& page_html);

private:
    struct LibraryPage
    {
        FileTime mtime;
        std::string page;
    };

    std::mutex cache_mutex;
    std::string index_page;
    FileTime index_mtime;
    std::unordered_map<std::string, LibraryPage> library_pages;
};

}

#endif
