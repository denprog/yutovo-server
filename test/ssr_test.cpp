/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <gtest/gtest.h>
#include "mock.h"
#include "../src/logic/ssr_html.h"

namespace yutovo_server_test
{

using namespace drogon;
using yutovo_server::SsrDocument;
using yutovo_server::SsrPage;

//Html escaping of the text and attribute values
TEST_F(SsrTest, escape)
{
    ASSERT_TRUE(SsrDocument::EscapeHtml("&<>\"'") == "&amp;&lt;&gt;&quot;&#39;");
    ASSERT_TRUE(SsrDocument::EscapeHtml("текст") == "текст");
}

//Localized heading format names
TEST_F(SsrTest, heading_level)
{
    ASSERT_TRUE(SsrDocument::HeadingLevel("Заголовок 4") == 4);
    ASSERT_TRUE(SsrDocument::HeadingLevel("Header 2") == 2);
    ASSERT_TRUE(SsrDocument::HeadingLevel("Encabezado 1") == 1);
    ASSERT_TRUE(SsrDocument::HeadingLevel("Основной текст") == 0);
    ASSERT_TRUE(SsrDocument::HeadingLevel("Text body") == 0);
}

//The lang attribute from the document language
TEST_F(SsrTest, lang)
{
    ASSERT_TRUE(SsrDocument::LangFromLanguage(1) == "en");
    ASSERT_TRUE(SsrDocument::LangFromLanguage(2) == "ru");
    ASSERT_TRUE(SsrDocument::LangFromLanguage(4) == "pt-BR");
    ASSERT_TRUE(SsrDocument::LangFromLanguage(0) == "");
}

//Headings, list grouping and text escaping in the article
TEST_F(SsrTest, article_semantic)
{
    Json::Value paragraphs;
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeString("Вводный текст & <тег>"))));
    paragraphs.append(MakeParagraph("Заголовок 4", MakeRow(MakeString("Секция"))));
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeString("первый")), "•"));
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeString("второй")), "•"));

    Json::Value document = MakeDocument(paragraphs);
    SsrDocument ssr(document);
    ASSERT_TRUE(ssr.GenerateArticle("Тестовый документ") ==
        "<article>"
            "<h1>Тестовый документ</h1>"
            "<p>"
                "<span style=\"font-family:'Arial';font-size:14px;\">Вводный текст &amp; &lt;тег&gt;</span>"
            "</p>"
            "<h4>"
                "<span style=\"font-family:'Arial';font-size:14px;\">Секция</span>"
            "</h4>"
            "<ul>"
                "<li>"
                    "<span style=\"font-family:'Arial';font-size:14px;\">первый</span>"
                "</li>"
                "<li>"
                    "<span style=\"font-family:'Arial';font-size:14px;\">второй</span>"
                "</li>"
            "</ul>"
        "</article>"
    ) << ssr.GenerateArticle("Тестовый документ");
}

//Formulas are generated as MathML
TEST_F(SsrTest, article_formula)
{
    //a / sqrt(b + b^2)
    Json::Value power = MakeLeftRight(15, MakeCodeRow(MakeCodeString("b")), MakeCodeRow(MakeCodeString("2")));

    Json::Value sum;
    sum["type"] = 7;
    sum["elements"].append(MakeCodeString("b"));
    sum["elements"].append(MakeOperator(11, "+"));
    sum["elements"].append(power);

    Json::Value root;
    root["type"] = 16;
    root["elements"].append(MakeShape());
    root["elements"].append(sum);

    Json::Value fraction = MakeLeftRight(14, MakeCodeRow(MakeCodeString("a")), root);

    Json::Value paragraphs;
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeCodeBlock(MakeCodeParagraph(fraction)))));

    Json::Value document = MakeDocument(paragraphs);
    SsrDocument ssr(document);
    ASSERT_TRUE(ssr.GenerateArticle("Формула") ==
        "<article>"
            "<h1>Формула</h1>"
            "<p>"
                "<math xmlns='http://www.w3.org/1998/Math/MathML'>"
                    "<mfrac>"
                        "<mrow>"
                            "<mi>a</mi>"
                        "</mrow>"
                        "<msqrt>"
                            "<mrow>"
                                "<mi>b</mi>"
                                "<mo>+</mo>"
                                "<msup>"
                                    "<mrow>"
                                        "<mi>b</mi>"
                                    "</mrow>"
                                    "<mrow>"
                                        "<mi>2</mi>"
                                    "</mrow>"
                                "</msup>"
                            "</mrow>"
                        "</msqrt>"
                    "</mfrac>"
                "</math>"
            "</p>"
        "</article>"
    ) << ssr.GenerateArticle("Формула");
}

//An equation with an unsolved result
TEST_F(SsrTest, article_equation)
{
    Json::Value result;
    result["type"] = 25;
    result["elements"] = Json::arrayValue;

    Json::Value right;
    right["type"] = 7;
    right["elements"].append(result);

    Json::Value equation = MakeLeftRight(18, MakeCodeRow(MakeCodeString("f")), right);
    equation["result_type"] = 5;

    Json::Value paragraphs;
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeCodeBlock(MakeCodeParagraph(equation)))));

    Json::Value document = MakeDocument(paragraphs);
    SsrDocument ssr(document);
    ASSERT_TRUE(ssr.GenerateArticle("Уравнение") ==
        "<article>"
            "<h1>Уравнение</h1>"
            "<p>"
                "<math xmlns='http://www.w3.org/1998/Math/MathML'>"
                    "<mrow>"
                        "<mi>f</mi>"
                    "</mrow>"
                    "<mo>=</mo>"
                    "<mrow>"
                        "<mrow></mrow>"
                    "</mrow>"
                "</math>"
            "</p>"
        "</article>"
    ) << ssr.GenerateArticle("Уравнение");
}

//Matrices and determinants are generated as mtable grids
TEST_F(SsrTest, article_matrix)
{
    auto make_matrix =
        [this](int type) -> Json::Value
        {
            Json::Value matrix;
            matrix["type"] = type;
            matrix["rows"] = 2;
            matrix["columns"] = 2;
            matrix["elements"].append(MakeShape());
            matrix["elements"].append(MakeCodeRow(MakeCodeString("1")));
            matrix["elements"].append(MakeCodeRow(MakeCodeString("2")));
            matrix["elements"].append(MakeCodeRow(MakeCodeString("3")));
            matrix["elements"].append(MakeCodeRow(MakeCodeString("4")));
            matrix["elements"].append(MakeShape());
            return matrix;
        };

    const std::string grid =
        "<mtable>"
            "<mtr>"
                "<mtd><mrow><mi>1</mi></mrow></mtd>"
                "<mtd><mrow><mi>2</mi></mrow></mtd>"
            "</mtr>"
            "<mtr>"
                "<mtd><mrow><mi>3</mi></mrow></mtd>"
                "<mtd><mrow><mi>4</mi></mrow></mtd>"
            "</mtr>"
        "</mtable>";

    Json::Value paragraphs;
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeCodeBlock(MakeCodeParagraph(make_matrix(60))))));
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeCodeBlock(MakeCodeParagraph(make_matrix(61))))));

    Json::Value document = MakeDocument(paragraphs);
    SsrDocument ssr(document);
    ASSERT_TRUE(ssr.GenerateArticle("Матрица") ==
        "<article>"
            "<h1>Матрица</h1>"
            "<p><math xmlns='http://www.w3.org/1998/Math/MathML'><mo>(</mo>" + grid + "<mo>)</mo></math></p>"
            "<p><math xmlns='http://www.w3.org/1998/Math/MathML'><mo>|</mo>" + grid + "<mo>|</mo></math></p>"
        "</article>"
    ) << ssr.GenerateArticle("Матрица");
}

//A limit renders as the lim text with the "variable → value" row under it
TEST_F(SsrTest, article_limit)
{
    Json::Value limit;
    limit["type"] = 62;
    limit["elements"].append(MakeCodeString("lim"));
    limit["elements"].append(MakeCodeRow(MakeCodeString("x")));
    limit["elements"].append(MakeCodeString("→"));
    limit["elements"].append(MakeCodeRow(MakeCodeString("0")));

    Json::Value paragraphs;
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeCodeBlock(MakeCodeParagraph(limit)))));

    Json::Value document = MakeDocument(paragraphs);
    SsrDocument ssr(document);
    ASSERT_TRUE(ssr.GenerateArticle("Предел") ==
        "<article>"
            "<h1>Предел</h1>"
            "<p><math xmlns='http://www.w3.org/1998/Math/MathML'>"
                "<munder>"
                    "<mi>lim</mi>"
                    "<mrow>"
                        "<mrow><mi>x</mi></mrow>"
                        "<mo>→</mo>"
                        "<mrow><mi>0</mi></mrow>"
                    "</mrow>"
                "</munder>"
            "</math></p>"
        "</article>"
    ) << ssr.GenerateArticle("Предел");
}

//Links and images
TEST_F(SsrTest, article_link_image)
{
    Json::Value link;
    link["type"] = 39;
    link["elements"] = "документация";
    link["url"] = "./Документация.yut";

    Json::Value image;
    image["type"] = 34;
    image["image_base64"] = "iVBORw0KGgo=";

    Json::Value row;
    row["type"] = 3;
    row["elements"].append(link);
    row["elements"].append(image);

    Json::Value paragraphs;
    paragraphs.append(MakeParagraph("Основной текст", row));

    Json::Value document = MakeDocument(paragraphs);
    SsrDocument ssr(document);
    ASSERT_TRUE(ssr.GenerateArticle("Ссылки") ==
        "<article>"
            "<h1>Ссылки</h1>"
            "<p>"
                "<a href=\"./Документация.yut\">документация</a>"
                "<img src=\"data:image/png;base64,iVBORw0KGgo=\">"
            "</p>"
        "</article>"
    ) << ssr.GenerateArticle("Ссылки");
}

//The meta description text
TEST_F(SsrTest, first_paragraph_text)
{
    Json::Value paragraphs;
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeString(""))));
    paragraphs.append(MakeParagraph("Заголовок 4", MakeRow(MakeString("Заголовок"))));
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeString("Первый содержательный абзац документа"))));

    Json::Value document = MakeDocument(paragraphs);
    SsrDocument ssr(document);
    ASSERT_TRUE(ssr.FirstParagraphText(160) == "Первый содержательный абзац документа");

    paragraphs.clear();
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(MakeString("abcdefghijklmnopqrstuvwxyz"))));
    Json::Value truncated_document = MakeDocument(paragraphs);
    SsrDocument ssr_truncated(truncated_document);
    ASSERT_TRUE(ssr_truncated.FirstParagraphText(10) == "abcdefghij…") << ssr_truncated.FirstParagraphText(10);
}

//The canonical host of the request: the yutovo.ru variants stay on yutovo.ru, the rest use yutovo.com
TEST_F(SsrTest, canonical_url)
{
    ASSERT_TRUE(SsrPage::CanonicalUrl("yutovo.ru", "/") == "https://yutovo.ru/");
    ASSERT_TRUE(SsrPage::CanonicalUrl("www.yutovo.ru:9001", "/library/Force.yut") == "https://yutovo.ru/library/Force.yut");
    ASSERT_TRUE(SsrPage::CanonicalUrl("YUTOVO.RU", "/") == "https://yutovo.ru/");
    ASSERT_TRUE(SsrPage::CanonicalUrl("yutovo.com", "/") == "https://yutovo.com/");
    ASSERT_TRUE(SsrPage::CanonicalUrl("www.yutovo.com:443", "/document/7") == "https://yutovo.com/document/7");
    ASSERT_TRUE(SsrPage::CanonicalUrl("example.org:8080", "/") == "https://yutovo.com/");
}

//The index page gets only the head links, the rest of the page is untouched
TEST_F(SsrTest, head_links)
{
    std::string index_html =
        "<html>"
            "<head>"
                "<title>Yutovo - visual online calculator</title>"
            "</head>"
            "<body>"
                "<div id=q-app></div>"
            "</body>"
        "</html>";

    std::string page = SsrPage::AddHeadLinks(index_html, "https://yutovo.com/");
    ASSERT_TRUE(page ==
        "<html>"
            "<head>"
                "<title>Yutovo - visual online calculator</title>"
                "<link rel=\"canonical\" href=\"https://yutovo.com/\">"
                "<link rel=\"alternate\" hreflang=\"ru-RU\" href=\"https://yutovo.ru/\">"
                "<link rel=\"alternate\" hreflang=\"x-default\" href=\"https://yutovo.com/\">"
            "</head>"
            "<body>"
                "<div id=q-app></div>"
            "</body>"
        "</html>"
    ) << page;

    ASSERT_TRUE(SsrPage::AddHeadLinks(index_html, "").empty());
    ASSERT_TRUE(SsrPage::AddHeadLinks("<html></html>", "https://yutovo.ru/").empty());
}

//The article is injected into the SPA page
TEST_F(SsrTest, page)
{
    std::string index_html =
        "<html>"
            "<head>"
                "<title>Yutovo - visual online calculator</title>"
                "<meta charset=utf-8>"
            "</head>"
            "<body>"
                "<div id=q-app></div>"
            "</body>"
        "</html>";

    std::string page = SsrPage::MakePage(index_html, "Частота генератора", "Описание документа", "ru",
        "<article><h1>Частота генератора</h1></article>", "https://yutovo.ru/library/ru/Frequency.yut");

    ASSERT_TRUE(page ==
        "<html lang=\"ru\">"
            "<head>"
                "<title>Частота генератора</title>"
                "<meta name=\"description\" content=\"Описание документа\">"
                "<link rel=\"canonical\" href=\"https://yutovo.ru/library/ru/Frequency.yut\">"
                "<link rel=\"alternate\" hreflang=\"ru-RU\" href=\"https://yutovo.ru/library/ru/Frequency.yut\">"
                "<link rel=\"alternate\" hreflang=\"x-default\" href=\"https://yutovo.com/library/ru/Frequency.yut\">"
                "<meta charset=utf-8>"
            "</head>"
            "<body>"
            "<div id=q-app>"
                "<canvas tabindex=\"-1\">"
                    "<article>"
                        "<h1>Частота генератора</h1>"
                    "</article>"
                "</canvas>"
            "</div>"
            "</body>"
        "</html>"
    ) << page;
}

//An old meta description is replaced
TEST_F(SsrTest, page_replace_description)
{
    std::string index_html =
        "<html>"
            "<head>"
                "<title>Old</title>"
                "<meta name=description content=\"old description\">"
            "</head>"
            "<body>"
                "<div id=\"q-app\"></div>"
            "</body>"
        "</html>";

    std::string page = SsrPage::MakePage(index_html, "Документ", "новое описание", "ru", "<article>x</article>",
        "https://yutovo.com/document/7");

    ASSERT_TRUE(page ==
        "<html lang=\"ru\">"
            "<head>"
                "<title>Документ</title>"
                "<meta name=\"description\" content=\"новое описание\">"
                "<link rel=\"canonical\" href=\"https://yutovo.com/document/7\">"
                "<link rel=\"alternate\" hreflang=\"ru-RU\" href=\"https://yutovo.ru/document/7\">"
                "<link rel=\"alternate\" hreflang=\"x-default\" href=\"https://yutovo.com/document/7\">"
            "</head>"
            "<body>"
            "<div id=\"q-app\">"
                "<canvas tabindex=\"-1\">"
                    "<article>x</article>"
                "</canvas>"
            "</div>"
            "</body>"
        "</html>"
    ) << page;
}

//Without the app container the page is not modified
TEST_F(SsrTest, page_no_container)
{
    ASSERT_TRUE(SsrPage::MakePage("<html><body></body></html>", "t", "d", "ru", "<article>x</article>", "").empty());
}

//The page is not corrupted when the document text contains the word "description"
TEST_F(SsrTest, page_description_word_in_article)
{
    std::string index_html =
        "<html>"
            "<head>"
                "<title>Yutovo</title>"
                "<meta charset=utf-8>"
                "<meta name=viewport content=\"width=device-width\">"
            "</head>"
            "<body>"
                "<div id=q-app></div>"
            "</body>"
        "</html>";

    std::string article = "<article><p>a document description field</p><p>tail</p></article>";
    std::string page = SsrPage::MakePage(index_html, "Document", "the description of the document", "en", article,
        "https://yutovo.ru/library/en/Document.yut");

    //the head tags survive and the article is injected intact
    ASSERT_TRUE(page ==
        "<html lang=\"en\">"
            "<head>"
                "<title>Document</title>"
                "<meta name=\"description\" content=\"the description of the document\">"
                "<link rel=\"canonical\" href=\"https://yutovo.ru/library/en/Document.yut\">"
                "<link rel=\"alternate\" hreflang=\"ru-RU\" href=\"https://yutovo.ru/library/en/Document.yut\">"
                "<link rel=\"alternate\" hreflang=\"x-default\" href=\"https://yutovo.com/library/en/Document.yut\">"
                "<meta charset=utf-8>"
                "<meta name=viewport content=\"width=device-width\">"
            "</head>"
            "<body>"
            "<div id=q-app>"
                "<canvas tabindex=\"-1\">"
                    "<article>"
                        "<p>a document description field</p>"
                        "<p>tail</p>"
                    "</article>"
                "</canvas>"
            "</div>"
            "</body>"
        "</html>"
    ) << page;
}

//An existing meta description is replaced even when the article mentions the word;
//an empty canonical url adds no head links
TEST_F(SsrTest, page_replace_description_word_in_article)
{
    std::string index_html =
        "<html>"
            "<head>"
                "<title>Old</title>"
                "<meta name=description content=\"old description\">"
                "<meta name=viewport content=\"width=device-width\">"
            "</head>"
            "<body>"
                "<div id=q-app></div>"
            "</body>"
        "</html>";

    std::string article = "<article><p>description inside the article</p></article>";
    std::string page = SsrPage::MakePage(index_html, "Документ", "новое описание", "ru", article, "");

    ASSERT_TRUE(page ==
        "<html lang=\"ru\">"
            "<head>"
                "<title>Документ</title>"
                "<meta name=\"description\" content=\"новое описание\">"
                "<meta name=viewport content=\"width=device-width\">"
            "</head>"
            "<body>"
            "<div id=q-app>"
                "<canvas tabindex=\"-1\">"
                    "<article>"
                        "<p>description inside the article</p>"
                    "</article>"
                "</canvas>"
            "</div>"
            "</body>"
        "</html>"
    ) << page;
}

//A list item keeps the alignment attribute, like the editor export
TEST_F(SsrTest, article_list_item_alignment)
{
    Json::Value paragraph = MakeParagraph("Основной текст", MakeRow(MakeString("пункт")), "•");
    paragraph["format_alignment"] = 2;

    Json::Value paragraphs;
    paragraphs.append(paragraph);

    Json::Value document = MakeDocument(paragraphs);
    SsrDocument ssr(document);
    ASSERT_TRUE(ssr.GenerateArticle("Список") ==
        "<article>"
            "<h1>Список</h1>"
            "<ul>"
                "<li align=\"center\">"
                    "<span style=\"font-family:'Arial';font-size:14px;\">пункт</span>"
                "</li>"
            "</ul>"
        "</article>"
    ) << ssr.GenerateArticle("Список");
}

//A chain of nested elements deeper than any real document must not overflow the stack;
//the article content is cut by the depth guard, so only the article frame is checked
TEST_F(SsrTest, article_deep_nesting_guard)
{
    Json::Value row;
    row["type"] = 7;
    for (int i = 0; i < 500; ++i)
    {
        Json::Value outer;
        outer["type"] = 7;
        outer["elements"].append(row);
        row = outer;
    }

    Json::Value paragraphs;
    paragraphs.append(MakeParagraph("Основной текст", MakeRow(row)));

    Json::Value document = MakeDocument(paragraphs);
    SsrDocument ssr(document);
    std::string article = ssr.GenerateArticle("Глубина");

    ASSERT_TRUE(article.find("<article>") != std::string::npos) << article;
    ASSERT_TRUE(article.find("</article>") != std::string::npos) << article;
}

//A library document page contains the full article with MathML
TEST_F(SessionTest, library_ssr_page)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/library/en/Units/Force.yut");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    std::string body(r->getBody());
    ASSERT_TRUE(body ==
        "<html lang=\"en\">"
            "<head>"
                "<title>Force</title>"
                "<meta name=\"description\" content=\"To convert a unit, edit the expression and select the desired unit in the result context menu:\">"
                "<link rel=\"canonical\" href=\"https://yutovo.ru/library/en/Units/Force.yut\">"
                "<link rel=\"alternate\" hreflang=\"ru-RU\" href=\"https://yutovo.ru/library/en/Units/Force.yut\">"
                "<link rel=\"alternate\" hreflang=\"x-default\" href=\"https://yutovo.com/library/en/Units/Force.yut\">"
            "</head>"
            "<body>"
                "<div id=q-app>"
                    "<canvas tabindex=\"-1\">"
                        "<article>"
                            "<h1>Force</h1>"
                            "<p>"
                                "<span style=\"font-family:'Arial';font-size:14px;\">To convert a unit, edit the expression and select the desired unit in the result context menu:</span>"
                            "</p>"
                            "<p>"
                                "<math xmlns='http://www.w3.org/1998/Math/MathML'>"
                                    "<mrow>"
                                        "<mrow>"
                                            "<mi>1</mi>"
                                            "<mi>N</mi>"
                                        "</mrow>"
                                        "<mo>=</mo>"
                                        "<mrow>"
                                            "<mrow></mrow>"
                                        "</mrow>"
                                    "</mrow>"
                                "</math>"
                            "</p>"
                            "<p>"
                                "<span style=\"font-family:'Arial';font-size:14px;\">Built-in units of force:</span>"
                            "</p>"
                            "<p>"
                                "<span style=\"white-space:nowrap; display:inline-block;line-height:2;vertical-align:top;\">"
                                    "<math xmlns='http://www.w3.org/1998/Math/MathML'><mrow><mrow><mi>1</mi><mi>mcN</mi></mrow><mo>=</mo><mrow><mrow></mrow></mrow></mrow></math><br>"
                                    "<math xmlns='http://www.w3.org/1998/Math/MathML'><mrow><mrow><mi>1</mi><mi>mN</mi></mrow><mo>=</mo><mrow><mrow></mrow></mrow></mrow></math><br>"
                                    "<math xmlns='http://www.w3.org/1998/Math/MathML'><mrow><mrow><mi>1</mi><mi>N</mi></mrow><mo>=</mo><mrow><mrow></mrow></mrow></mrow></math><br>"
                                    "<math xmlns='http://www.w3.org/1998/Math/MathML'><mrow><mrow><mi>1</mi><mi>kN</mi></mrow><mo>=</mo><mrow><mrow></mrow></mrow></mrow></math><br>"
                                    "<math xmlns='http://www.w3.org/1998/Math/MathML'><mrow><mrow><mi>1</mi><mi>MN</mi></mrow><mo>=</mo><mrow><mrow></mrow></mrow></mrow></math>"
                                "</span>"
                            "</p>"
                        "</article>"
                    "</canvas>"
                "</div>"
            "</body>"
        "</html>\n"
    ) << body;
}

//A not found library document returns the plain SPA page
TEST_F(SessionTest, library_ssr_page_not_found)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/library/en/Units/No such document.yut");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k404NotFound) << r->getStatusCode();

    std::string body(r->getBody());
    ASSERT_TRUE(body ==
        "<html>"
            "<head>"
                "<title>Yutovo - visual online calculator</title>"
            "</head>"
            "<body>"
                "<div id=q-app></div>"
            "</body>"
        "</html>\n"
    ) << body;
}

//The include_documents are not expanded: the article contains only the document itself
TEST_F(SessionTest, library_ssr_page_no_includes)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/library/ru/Калькуляторы/Финансы/Вклады/Простой%20процент.yut");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    std::string body(r->getBody());
    ASSERT_TRUE(body.find("<article>") != std::string::npos) << body.substr(0, 500);

    //the intro paragraph occurs twice: in the stub's own copy and in the meta description; a third occurrence would mean the include was expanded
    const std::string intro = "Вклад с простым процентом - без капитализации";
    size_t count = 0;
    for (size_t pos = body.find(intro); pos != std::string::npos; pos = body.find(intro, pos + 1))
        ++count;
    ASSERT_TRUE(count == 2) << count << " " << body.substr(0, 300);
}

//The sitemap is generated from the library files with working document urls
TEST_F(SessionTest, sitemap)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/sitemap.xml");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    ASSERT_TRUE(r->getContentType() == CT_APPLICATION_XML) << r->getContentType();

    //the test server host is www.yutovo.ru:9001 and the scheme defaults to https;
    std::string body(r->getBody());
    std::string header =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n"
        "<url><loc>https://www.yutovo.ru:9001/</loc></url>\n";
    ASSERT_TRUE(body.compare(0, header.size(), header) == 0) << body.substr(0, 300);
    ASSERT_TRUE(body.find("<loc>https://www.yutovo.ru:9001/library/de/") != std::string::npos) << body.substr(0, 300);
    ASSERT_TRUE(body.find("<loc>https://www.yutovo.ru:9001/library/en/Units/Force.yut</loc>") != std::string::npos) << body.substr(0, 300);
    //non-ascii names and spaces are percent encoded, the extension is kept
    ASSERT_TRUE(body.find("%D0%9E%20%D0%AE%D1%82%D0%BE%D0%B2%D0%BE.yut</loc>") != std::string::npos) << body.substr(0, 300);
    ASSERT_TRUE(body.find("<lastmod>") != std::string::npos) << body.substr(0, 300);
}

//A public user document page contains the article, a private one does not
TEST_F(SessionTest, user_document_ssr_page)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    Register(client, "UserSsr", "user_ssr@mail.com", "11");

    std::string access_token, document_id, name, language;
    Login(client, "UserSsr", "11", access_token, document_id, name, language);

    std::ifstream f("../../tests/files11.yut");
    Json::Value save_body;
    f >> save_body;
    auto req = HttpRequest::newHttpJsonRequest(save_body);
    req->setMethod(drogon::Post);
    req->setPath("/service/save-document");
    req->addHeader("access_token", access_token);
    auto resp = client->sendRequest(req, 10);
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    //a fixed name makes the expected page deterministic
    RenameDocument(client, access_token, document_id, "SSR document");

    //public documents (the default) have the SEO content
    auto get = HttpRequest::newHttpRequest();
    get->setMethod(drogon::Get);
    get->setPath("/document/" + document_id);
    resp = client->sendRequest(get);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    std::string body(r->getBody());
    ASSERT_TRUE(body ==
        "<html>"
            "<head>"
                "<title>SSR document</title>"
                "<meta name=\"description\" content=\"Text red blue\">"
                "<link rel=\"canonical\" href=\"https://yutovo.ru/document/" + document_id + "\">"
                "<link rel=\"alternate\" hreflang=\"ru-RU\" href=\"https://yutovo.ru/document/" + document_id + "\">"
                "<link rel=\"alternate\" hreflang=\"x-default\" href=\"https://yutovo.com/document/" + document_id + "\">"
            "</head>"
            "<body>"
                "<div id=q-app>"
                    "<canvas tabindex=\"-1\">"
                        "<article>"
                            "<h1>SSR document</h1>"
                            "<p>"
                                "<span style=\"font-family:'Arial';font-size:14px;\">Text </span>"
                                "<span style=\"font-family:'Arial';font-size:14px;color:#ff0000;\">red </span>"
                                "<span style=\"font-family:'Arial';font-size:14px;color:#ff0000;background:#0000ff;\">blue</span>"
                            "</p>"
                        "</article>"
                    "</canvas>"
                "</div>"
            "</body>"
        "</html>\n"
    ) << body;

    //private documents do not expose the content
    orm::DbClientPtr db = app().getDbClient();
    db->execSqlSync("update user_documents set public=false where document_id=$1", document_id);
    resp = client->sendRequest(get);
    r = resp.second;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();
    body = std::string(r->getBody());
    ASSERT_TRUE(body ==
        "<html>"
            "<head>"
                "<title>Yutovo - visual online calculator</title>"
            "</head>"
            "<body>"
                "<div id=q-app></div>"
            "</body>"
        "</html>\n"
    ) << body;

    UnRegister(client, "UserSsr", access_token);
}

//The root page gets the canonical and hreflang head links of the requested host
TEST_F(SessionTest, root_page)
{
    auto client = HttpClient::newHttpClient(address);
    client->enableCookies(true);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/");

    auto resp = client->sendRequest(req);
    ReqResult& res = resp.first;
    HttpResponsePtr& r = resp.second;
    ASSERT_TRUE(res == ReqResult::Ok) << res;
    ASSERT_TRUE(r->getStatusCode() == k200OK) << r->getStatusCode();

    std::string body(r->getBody());
    ASSERT_TRUE(body ==
        "<html>"
            "<head>"
                "<title>Yutovo - visual online calculator</title>"
                "<link rel=\"canonical\" href=\"https://yutovo.ru/\">"
                "<link rel=\"alternate\" hreflang=\"ru-RU\" href=\"https://yutovo.ru/\">"
                "<link rel=\"alternate\" hreflang=\"x-default\" href=\"https://yutovo.com/\">"
            "</head>"
            "<body>"
                "<div id=q-app></div>"
            "</body>"
        "</html>\n"
    ) << body;
}

}
