/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "ssr_html.h"
#include <fstream>
#include <sstream>
#include <functional>
#include <cstdio>
#include <drogon/HttpAppFramework.h>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/device/array.hpp>

namespace yutovo_server
{

namespace fs = std::filesystem;

constexpr int TYPE_NONE = 0;
constexpr int TYPE_TEXT = 1;
constexpr int TYPE_PARAGRAPH = 2;
constexpr int TYPE_ROW = 3;
constexpr int TYPE_STRING = 4;
constexpr int TYPE_CODE_BLOCK = 5;
constexpr int TYPE_CODE_PARAGRAPH = 6;
constexpr int TYPE_CODE_ROW = 7;
constexpr int TYPE_CODE_STRING = 8;
constexpr int TYPE_CODE_COLUMN = 9;
constexpr int TYPE_SHAPE = 10;
constexpr int TYPE_PLUS = 11;
constexpr int TYPE_MINUS = 12;
constexpr int TYPE_MULTIPLY = 13;
constexpr int TYPE_DIVISION = 14;
constexpr int TYPE_POWER = 15;
constexpr int TYPE_SQUARE_ROOT = 16;
constexpr int TYPE_NTH_ROOT = 17;
constexpr int TYPE_EQUATION = 18;
constexpr int TYPE_OPEN_ROUND_BRACKET = 19;
constexpr int TYPE_CLOSE_ROUND_BRACKET = 20;
constexpr int TYPE_REAL_RESULT = 21;
constexpr int TYPE_INTEGER_RESULT = 22;
constexpr int TYPE_RATIONAL_RESULT = 23;
constexpr int TYPE_COMPLEX_RESULT = 24;
constexpr int TYPE_AUTO_RESULT = 25;
constexpr int TYPE_ERROR_RESULT = 26;
constexpr int TYPE_ASSIGNMENT = 27;
constexpr int TYPE_SUBSCRIPT = 28;
constexpr int TYPE_EXCLAMATION = 29;
constexpr int TYPE_AND = 30;
constexpr int TYPE_OR = 31;
constexpr int TYPE_XOR = 32;
constexpr int TYPE_PERCENT = 33;
constexpr int TYPE_IMAGE = 34;
constexpr int TYPE_SUM = 35;
constexpr int TYPE_PRODUCT = 36;
constexpr int TYPE_UNIT = 37;
constexpr int TYPE_COMMA = 38;
constexpr int TYPE_LINK = 39;
constexpr int TYPE_OPEN_SQUARE_BRACKET = 40;
constexpr int TYPE_CLOSE_SQUARE_BRACKET = 41;
constexpr int TYPE_ARRAY_REAL_RESULT = 42;
constexpr int TYPE_GRAPH_LINE = 43;
constexpr int TYPE_CODE_PARAGRAPHS_BLOCK = 44;
constexpr int TYPE_SYMBOLIC_REAL_RESULT = 45;
constexpr int TYPE_SYMBOLIC_RATIONAL_RESULT = 46;
constexpr int TYPE_SYMBOLIC_COMPLEX_RESULT = 47;
constexpr int TYPE_NOT = 48;
constexpr int TYPE_DEFINITE_INTEGRAL = 49;
constexpr int TYPE_INDEFINITE_INTEGRAL = 50;
constexpr int TYPE_CODE_ROW_ASSIGNMENT = 51;
constexpr int TYPE_CODE_PARAGRAPH_ASSIGNMENT = 52;
constexpr int TYPE_CODE_PARAGRAPHS_BLOCK_ASSIGNMENT = 53;
constexpr int TYPE_EVALUATION_BAR_SUBSCRIPT = 54;
constexpr int TYPE_TEXT_BLOCK = 55;
constexpr int TYPE_TEXT_EQUATION = 56;
constexpr int TYPE_TEXT_ASSIGNMENT = 57;
constexpr int TYPE_GRAPH_SURFACE = 58;
constexpr int TYPE_GRAPH_HISTOGRAM = 59;
constexpr int TYPE_MATRIX = 60;
constexpr int TYPE_DETERMINANT = 61;
constexpr int TYPE_LIMIT = 62;

const Json::Value empty_value;

//SsrDocument

SsrDocument::SsrDocument(const Json::Value& document) :
    document(document)
{
    const Json::Value& formats = document["string_formats"];
    if (formats.isArray())
    {
        for (const auto& format : formats)
        {
            const Json::Value& id = format["id"];
            if (id.isString())
                string_formats[id.asString()] = &format;
        }
    }
}

std::string SsrDocument::EscapeHtml(const std::string& str)
{
    std::string s;
    s.reserve(str.size());
    for (char c : str)
    {
        switch (c)
        {
        case '&':
            s += "&amp;";
            break;
        case '<':
            s += "&lt;";
            break;
        case '>':
            s += "&gt;";
            break;
        case '\"':
            s += "&quot;";
            break;
        case '\'':
            s += "&#39;";
            break;
        default:
            s += c;
            break;
        }
    }
    return s;
}

int SsrDocument::HeadingLevel(const std::string& format_name)
{
    //localized paragraph format names, see the translation table in yutovo-library/AGENTS.md
    static const std::map<std::string, int> headings =
        {
            {"Header 1", 1}, {"Header 2", 2}, {"Header 3", 3}, {"Header 4", 4},
            {"Заголовок 1", 1}, {"Заголовок 2", 2}, {"Заголовок 3", 3}, {"Заголовок 4", 4},
            {"Encabezado 1", 1}, {"Encabezado 2", 2}, {"Encabezado 3", 3}, {"Encabezado 4", 4},
            {"Cabeçalho 1", 1}, {"Cabeçalho 2", 2}, {"Cabeçalho 3", 3}, {"Cabeçalho 4", 4},
            {"Überschrift 1", 1}, {"Überschrift 2", 2}, {"Überschrift 3", 3}, {"Überschrift 4", 4},
            {"Titre 1", 1}, {"Titre 2", 2}, {"Titre 3", 3}, {"Titre 4", 4},
            {"Titolo 1", 1}, {"Titolo 2", 2}, {"Titolo 3", 3}, {"Titolo 4", 4}
        };

    auto it = headings.find(format_name);
    if (it == headings.end())
        return 0;
    return it->second;
}

std::string SsrDocument::LangFromLanguage(int language)
{
    switch (language)
    {
    case 1:
        return "en";
    case 2:
        return "ru";
    case 3:
        return "es";
    case 4:
        return "pt-BR";
    case 5:
        return "de";
    case 6:
        return "fr";
    case 7:
        return "it";
    default:
        return "";
    }
}

bool SsrDocument::ReadLibraryFile(const std::string& path, Json::Value& document)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (content.empty())
        return false;

    //the library files may be gzip compressed or plain json
    try
    {
        boost::iostreams::array_source src(content.data(), content.size());
        boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
        in.push(boost::iostreams::gzip_decompressor());
        in.push(src);
        std::stringstream json;
        boost::iostreams::copy(in, json);

        Json::Reader reader;
        return reader.parse(json.str(), document) && document.isObject();
    }
    catch (const std::ios_base::failure&)
    {
    }

    Json::Reader reader;
    return reader.parse(content, document) && document.isObject();
}

std::string SsrDocument::GenerateArticle(const std::string& title) const
{
    std::string article = "<article>";
    if (!title.empty())
        article += "<h1>" + EscapeHtml(title) + "</h1>";
    article += ParagraphsToHtml({&document["text"]["elements"]});
    article += "</article>";
    return article;
}

std::string SsrDocument::ParagraphsToHtml(const std::vector<const Json::Value*>& element_arrays) const
{
    std::string html;
    bool in_list = false;
    for (const Json::Value* elements : element_arrays)
    {
        if (!elements || !elements->isArray())
            continue;

        for (const auto& element : *elements)
        {
            bool marker = element["type"].asInt() == TYPE_PARAGRAPH && IsListMarker(element.get("marker", "").asString());
            if (marker)
            {
                if (!in_list)
                {
                    html += "<ul>";
                    in_list = true;
                }
                html += ListItemToHtml(element);
            }
            else
            {
                if (in_list)
                {
                    html += "</ul>";
                    in_list = false;
                }
                html += ElementToHtml(element);
            }
        }
    }
    if (in_list)
        html += "</ul>";
    return html;
}

std::string SsrDocument::FirstParagraphText(size_t max_length) const
{
    const Json::Value& elements = document["text"]["elements"];
    if (!elements.isArray())
        return "";

    for (const auto& element : elements)
    {
        if (element["type"].asInt() != TYPE_PARAGRAPH)
            continue;

        //headings are not used for the description
        if (HeadingLevel(element.get("format_name", "").asString()) > 0)
            continue;

        //collect the plain strings of the paragraph, skipping code and formulas
        std::string text;
        std::function<void(const Json::Value&)> collect =
            [&](const Json::Value& el)
            {
                const Json::Value& children = el["elements"];
                if (!children.isArray())
                    return;
                for (const auto& child : children)
                {
                    int type = child["type"].asInt();
                    if (type == TYPE_STRING && child["elements"].isString())
                        text += child["elements"].asString();
                    else if (type == TYPE_ROW || type == TYPE_TEXT)
                        collect(child);
                }
            };
        collect(element);

        //trim
        size_t begin = text.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
            continue;
        size_t end = text.find_last_not_of(" \t\r\n");
        text = text.substr(begin, end - begin + 1);

        if (text.size() > max_length)
        {
            size_t length = max_length;
            //do not cut inside a utf-8 sequence
            while (length > 0 && (static_cast<unsigned char>(text[length]) & 0xc0) == 0x80)
                --length;
            text = text.substr(0, length) + "…";
        }
        return text;
    }
    return "";
}

int SsrDocument::Language() const
{
    const Json::Value& language = document["config"]["language"];
    if (!language.isInt())
        return 0;
    return language.asInt();
}

std::string SsrDocument::ElementsToHtml(const Json::Value& element) const
{
    const Json::Value& children = element["elements"];
    if (!children.isArray())
        return "";

    std::string html;
    for (const auto& child : children)
        html += ElementToHtml(child);
    return html;
}

std::string SsrDocument::ElementToHtml(const Json::Value& element) const
{
    if (!element.isObject())
        return "";
    if (depth >= max_element_depth)
        return "";

    DepthGuard guard(this);

    switch (element["type"].asInt())
    {
    case TYPE_TEXT:
        return ElementsToHtml(element);
    case TYPE_PARAGRAPH:
        return ParagraphToHtml(element);
    case TYPE_ROW:
        return ElementsToHtml(element);
    case TYPE_CODE_COLUMN:
        return ElementsToHtml(element);
    case TYPE_STRING:
        return StringToHtml(element);
    case TYPE_CODE_BLOCK:
    case TYPE_TEXT_BLOCK:
        return CodeBlockToHtml(element);
    case TYPE_CODE_PARAGRAPH:
    case TYPE_CODE_PARAGRAPH_ASSIGNMENT:
        return CodeParagraphToHtml(element);
    case TYPE_CODE_ROW:
    case TYPE_CODE_ROW_ASSIGNMENT:
        return "<mrow>" + ElementsToHtml(element) + "</mrow>";
    case TYPE_CODE_STRING:
        return "<mi>" + EscapeHtml(element["elements"].asString()) + "</mi>";
    case TYPE_NONE:
    case TYPE_SHAPE:
        return "";
    case TYPE_PLUS:
        return OperatorToHtml(element, "+");
    case TYPE_MINUS:
        return OperatorToHtml(element, "-");
    case TYPE_MULTIPLY:
        return OperatorToHtml(element, "·");
    case TYPE_DIVISION:
        return "<mfrac>" + ElementToHtml(Child(element, 0)) + ElementToHtml(Child(element, 2)) + "</mfrac>";
    case TYPE_POWER:
        return "<msup>" + ElementToHtml(Child(element, 0)) + ElementToHtml(Child(element, 2)) + "</msup>";
    case TYPE_SQUARE_ROOT:
        return "<msqrt>" + ElementToHtml(Child(element, 1)) + "</msqrt>";
    case TYPE_NTH_ROOT:
        return "<mroot>" + ElementToHtml(Child(element, 2)) + ElementToHtml(Child(element, 0)) + "</mroot>";
    case TYPE_EQUATION:
    case TYPE_TEXT_EQUATION:
        return LeftRightToHtml(element, "=");
    case TYPE_OPEN_ROUND_BRACKET:
        return OperatorToHtml(element, "(");
    case TYPE_CLOSE_ROUND_BRACKET:
        return OperatorToHtml(element, ")");
    case TYPE_OPEN_SQUARE_BRACKET:
        return OperatorToHtml(element, "[");
    case TYPE_CLOSE_SQUARE_BRACKET:
        return OperatorToHtml(element, "]");
    case TYPE_COMMA:
        return OperatorToHtml(element, ",");
    case TYPE_REAL_RESULT:
    case TYPE_INTEGER_RESULT:
    case TYPE_RATIONAL_RESULT:
    case TYPE_COMPLEX_RESULT:
    case TYPE_AUTO_RESULT:
    case TYPE_ERROR_RESULT:
    case TYPE_ARRAY_REAL_RESULT:
    case TYPE_SYMBOLIC_REAL_RESULT:
    case TYPE_SYMBOLIC_RATIONAL_RESULT:
    case TYPE_SYMBOLIC_COMPLEX_RESULT:
        return ResultToHtml(element);
    case TYPE_ASSIGNMENT:
    case TYPE_TEXT_ASSIGNMENT:
        return LeftRightToHtml(element, "=");
    case TYPE_UNIT:
        return LeftRightToHtml(element, "~");
    case TYPE_SUBSCRIPT:
        return "<msub>" + ElementToHtml(Child(element, 0)) + ElementToHtml(Child(element, 2)) + "</msub>";
    case TYPE_EXCLAMATION:
        return "<mo>!</mo>";
    case TYPE_AND:
        return "<mo>∧</mo>";
    case TYPE_OR:
        return "<mo>∨</mo>";
    case TYPE_XOR:
        return "<mo>⊕</mo>";
    case TYPE_PERCENT:
        return "<mo>%</mo>";
    case TYPE_NOT:
        return "<mo>¬</mo>";
    case TYPE_IMAGE:
        return "<img src=\"data:image/png;base64," + EscapeHtml(element["image_base64"].asString()) + "\">";
    case TYPE_SUM:
        return IterationToHtml(element, "∑");
    case TYPE_PRODUCT:
        return IterationToHtml(element, "∏");
    case TYPE_LINK:
        return LinkToHtml(element);
    case TYPE_GRAPH_LINE:
    case TYPE_GRAPH_SURFACE:
    case TYPE_GRAPH_HISTOGRAM:
        return GraphToHtml(element);
    case TYPE_CODE_PARAGRAPHS_BLOCK:
    case TYPE_CODE_PARAGRAPHS_BLOCK_ASSIGNMENT:
        return CodeParagraphsBlockToHtml(element);
    case TYPE_DEFINITE_INTEGRAL:
        return DefiniteIntegralToHtml(element);
    case TYPE_INDEFINITE_INTEGRAL:
        return IndefiniteIntegralToHtml(element);
    case TYPE_LIMIT:
        return LimitToHtml(element);
    case TYPE_EVALUATION_BAR_SUBSCRIPT:
        return EvaluationBarToHtml(element);
    case TYPE_MATRIX:
    case TYPE_DETERMINANT:
        return MatrixToHtml(element);
    default:
        return "";
    }
}

std::string SsrDocument::ParagraphToHtml(const Json::Value& element) const
{
    std::string tag = "p";
    int level = HeadingLevel(element.get("format_name", "").asString());
    if (level > 0)
        tag = "h" + std::to_string(level);

    return "<" + tag + AlignmentAttributes(element) + ">" + ElementsToHtml(element) + "</" + tag + ">";
}

std::string SsrDocument::ListItemToHtml(const Json::Value& element) const
{
    //like the editor export, the alignment attribute is kept on list items
    return "<li" + AlignmentAttributes(element) + ">" + ElementsToHtml(element) + "</li>";
}

std::string SsrDocument::StringToHtml(const Json::Value& element) const
{
    std::string text = EscapeHtml(element["elements"].asString());
    const Json::Value* format = FindStringFormat(element);
    if (!format)
        return text;

    bool bold = format->get("bold", false).asBool();
    bool italic = format->get("italic", false).asBool();
    bool subscript = format->get("subscript", false).asBool();
    bool superscript = format->get("superscript", false).asBool();

    std::string s = "<span " + StringStyle(format) + ">";
    if (subscript)
        s += "<sub>";
    if (superscript)
        s += "<sup>";
    if (bold)
        s += "<strong>";
    if (italic)
        s += "<em>";
    s += text;
    if (italic)
        s += "</em>";
    if (bold)
        s += "</strong>";
    if (superscript)
        s += "</sup>";
    if (subscript)
        s += "</sub>";
    s += "</span>";
    return s;
}

std::string SsrDocument::LinkToHtml(const Json::Value& element) const
{
    //spaces and control characters are not allowed in the url
    std::string url;
    for (char c : element["url"].asString())
    {
        unsigned char u = static_cast<unsigned char>(c);
        if (u <= ' ')
            continue;
        url += c;
    }

    const Json::Value* format = FindStringFormat(element);
    std::string style;
    if (format)
        style = " " + StringStyle(format);

    return "<a href=\"" + EscapeHtml(url) + "\"" + style + ">" + EscapeHtml(element["elements"].asString()) + "</a>";
}

std::string SsrDocument::CodeParagraphToHtml(const Json::Value& element) const
{
    return "<math xmlns='http://www.w3.org/1998/Math/MathML'>" + ElementsToHtml(element) + "</math>";
}

std::string SsrDocument::CodeBlockToHtml(const Json::Value& element) const
{
    const Json::Value& children = element["elements"];
    if (!children.isArray() || children.size() == 0)
        return "";
    if (children.size() == 1)
        return ElementToHtml(children[0]);

    std::string html = "<span style=\"white-space:nowrap; display:inline-block;line-height:2;vertical-align:top;\">";
    for (const auto& child : children)
    {
        html += ElementToHtml(child);
        if (&child != &children[children.size() - 1])
            html += "<br>";
    }
    html += "</span>";
    return html;
}

std::string SsrDocument::CodeParagraphsBlockToHtml(const Json::Value& element) const
{
    const Json::Value& children = element["elements"];
    if (!children.isArray() || children.size() == 0)
        return "";
    if (children.size() == 1)
        return ElementToHtml(children[0]);

    std::string html;
    for (const auto& child : children)
        html += "<p>" + ElementToHtml(child) + "</p>";
    return html;
}

std::string SsrDocument::OperatorToHtml(const Json::Value& element, const std::string& default_symbol) const
{
    const Json::Value& symbol = element["symbol"];
    return "<mo>" + EscapeHtml(symbol.isString() ? symbol.asString() : default_symbol) + "</mo>";
}

std::string SsrDocument::LeftRightToHtml(const Json::Value& element, const std::string& symbol) const
{
    std::string s = ElementToHtml(Child(element, 0));
    s += "<mo>" + symbol + "</mo>";
    const Json::Value& children = element["elements"];
    if (children.isArray() && children.size() > 2)
        s += ElementToHtml(children[2]);
    return s;
}

std::string SsrDocument::ResultToHtml(const Json::Value& element) const
{
    //the results are solved on the client; a saved document may keep the value in the first child
    const Json::Value& children = element["elements"];
    if (children.isArray() && children.size() > 0)
        return ElementToHtml(children[0]);
    return "<mrow></mrow>";
}

std::string SsrDocument::IterationToHtml(const Json::Value& element, const std::string& symbol) const
{
    std::string s = "<munderover><mo>" + symbol + "</mo>";
    s += "<mrow>" + ElementToHtml(Child(element, 0)) + "</mrow>";
    s += "<mrow>" + ElementToHtml(Child(element, 2)) + "</mrow>";
    s += "</munderover>";
    s += ElementToHtml(Child(element, 3));
    return s;
}

std::string SsrDocument::DefiniteIntegralToHtml(const Json::Value& element) const
{
    std::string s = "<munderover><mo>∫</mo>";
    s += "<mrow>" + ElementToHtml(Child(element, 0)) + "</mrow>";
    s += "<mrow>" + ElementToHtml(Child(element, 2)) + "</mrow>";
    s += "</munderover>";
    s += ElementToHtml(Child(element, 3));
    s += ElementToHtml(Child(element, 4));
    s += ElementToHtml(Child(element, 5));
    return s;
}

std::string SsrDocument::IndefiniteIntegralToHtml(const Json::Value& element) const
{
    std::string s = "<mo>∫</mo>";
    s += ElementToHtml(Child(element, 1));
    s += ElementToHtml(Child(element, 2));
    s += ElementToHtml(Child(element, 3));
    return s;
}

std::string SsrDocument::LimitToHtml(const Json::Value& element) const
{
    std::string s = "<munder><mi>lim</mi><mrow>";
    s += ElementToHtml(Child(element, 1));
    s += "<mo>→</mo>";
    s += ElementToHtml(Child(element, 3));
    s += "</mrow></munder>";
    return s;
}

std::string SsrDocument::MatrixToHtml(const Json::Value& element) const
{
    unsigned rows = element.get("rows", 0).asUInt();
    unsigned columns = element.get("columns", 0).asUInt();
    if (rows == 0 || columns == 0)
        return "";

    const Json::Value& children = element["elements"];
    if (!children.isArray() || children.size() != 2 + rows * columns)
        return "";

    bool bars = element["type"].asInt() == TYPE_DETERMINANT;
    std::string open_fence = bars ? "<mo>|</mo>" : "<mo>(</mo>";
    std::string close_fence = bars ? "<mo>|</mo>" : "<mo>)</mo>";

    std::string s = open_fence + "<mtable>";
    for (unsigned r = 0; r < rows; ++r)
    {
        s += "<mtr>";
        for (unsigned c = 0; c < columns; ++c)
            s += "<mtd>" + ElementToHtml(Child(element, 1 + r * columns + c)) + "</mtd>";
        s += "</mtr>";
    }
    s += "</mtable>" + close_fence;
    return s;
}

std::string SsrDocument::EvaluationBarToHtml(const Json::Value& element) const
{
    std::string sub;
    const Json::Value& children = element["elements"];
    if (children.isArray())
    {
        bool first = true;
        for (const auto& child : children)
        {
            if (child["type"].asInt() != TYPE_CODE_PARAGRAPHS_BLOCK && child["type"].asInt() != TYPE_CODE_PARAGRAPHS_BLOCK_ASSIGNMENT)
                continue;

            const Json::Value& paragraphs = child["elements"];
            if (!paragraphs.isArray())
                continue;
            for (const auto& paragraph : paragraphs)
            {
                const Json::Value& rows = paragraph["elements"];
                if (!rows.isArray())
                    continue;
                for (const auto& row : rows)
                {
                    std::string row_html = ElementToHtml(row);
                    if (row_html.empty())
                        continue;
                    if (!first)
                        sub += "<mo>,</mo>";
                    first = false;
                    sub += row_html;
                }
            }
        }
    }

    return "<msub><mo fence=\"false\" stretchy=\"true\">|</mo><mrow>" + sub + "</mrow></msub>";
}

std::string SsrDocument::GraphToHtml(const Json::Value& element) const
{
    int type = element["type"].asInt();
    const Json::Value& expressions = Child(element, type == TYPE_GRAPH_HISTOGRAM ? 0 : 1);
    const Json::Value& plots = element["plots"];

    std::string expr;
    const Json::Value& paragraphs = expressions["elements"];
    if (paragraphs.isArray())
    {
        Json::ArrayIndex i = 0;
        for (const auto& paragraph : paragraphs)
        {
            //the color marker of the plot, the plot may be missing in an unsolved template
            unsigned int color = plots[i]["plot_format"]["color"].isUInt() ? plots[i]["plot_format"]["color"].asUInt() : 0;
            expr += "<p>";
            if (color != 0)
                expr += "<span style=\"color: " + ColorToHtml(color) + ";\">█&nbsp;</span>";
            expr += ElementToHtml(paragraph) + "</p>";
            ++i;
        }
    }

    if (type == TYPE_GRAPH_HISTOGRAM)
        return expr;

    //the table layout of the editor without the image: the server does not render graphs
    unsigned variable_index = type == TYPE_GRAPH_SURFACE ? 5 : 2;
    std::string s = "<table>";
    s += "<tr><td style=\"vertical-align:top;text-align:right;\">" + ElementToHtml(Child(element, 0)) + "</td></tr>";
    s += "<tr><td>" + expr + "</td></tr>";
    if (type == TYPE_GRAPH_SURFACE)
        s += "<tr><td style=\"text-align:right;\">" + ElementToHtml(Child(element, 2)) + "</td></tr>";
    s += "<tr><td style=\"vertical-align:bottom;text-align:right;\">" + ElementToHtml(Child(element, 3)) + "</td></tr>";
    s += "</table>";
    s += "<table style=\"width:100%;\"><tr>";
    s += "<td style=\"vertical-align:top;text-align:left;\">" + ElementToHtml(Child(element, 4)) + "</td>";
    s += "<td style=\"vertical-align:top;text-align:center;\">" + ElementToHtml(Child(element, variable_index)) + "</td>";
    s += "<td style=\"vertical-align:top;text-align:right;\">" + ElementToHtml(Child(element, type == TYPE_GRAPH_SURFACE ? 6 : 5)) + "</td>";
    s += "</tr></table>";
    return s;
}

std::string SsrDocument::StringStyle(const Json::Value* format) const
{
    std::string family;
    for (char c : format->get("family", "").asString())
    {
        unsigned char u = static_cast<unsigned char>(c);
        if (c == '\'' || c == '\\' || u < ' ' || u == 0x7f)
            continue;
        family += c;
    }

    std::string s = "style=\"font-family:'" + EscapeHtml(family) + "';";
    s += "font-size:" + std::to_string(format->get("size", 0).asInt()) + "px;";
    if (format->get("underline", false).asBool())
        s += "text-decoration: underline;";
    if (format->get("strikethrough", false).asBool())
        s += "text-decoration: line-through;";
    unsigned int text_color = ColorFromJson(format->get("text_color", Json::Value()), 0);
    if (text_color != 0 && text_color != 4278190080) //not the default black
        s += "color:" + ColorToHtml(text_color) + ";";
    unsigned int text_bg_color = ColorFromJson(format->get("text_bg_color", Json::Value()), 4294967295);
    if (text_bg_color != 4294967295) //not the default white
        s += "background:" + ColorToHtml(text_bg_color) + ";";
    s += "\"";
    return s;
}

const Json::Value* SsrDocument::FindStringFormat(const Json::Value& element) const
{
    const Json::Value& format_id = element["format_id"];
    if (!format_id.isString())
        return nullptr;
    auto it = string_formats.find(format_id.asString());
    if (it == string_formats.end())
        return nullptr;
    return it->second;
}

std::string SsrDocument::ColorToHtml(unsigned int color)
{
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%06x", color & 0xffffff);
    return std::string(buffer);
}

unsigned int SsrDocument::ColorFromJson(const Json::Value& value, unsigned int default_color)
{
    //the colors are unsigned ARGB integers, but a negative int is accepted too
    if (value.isUInt())
        return value.asUInt();
    if (value.isInt())
        return static_cast<unsigned int>(value.asInt());
    return default_color;
}

std::string SsrDocument::AlignmentAttributes(const Json::Value& element)
{
    switch (element.get("format_alignment", 0).asInt())
    {
    case 1:
        return " align=\"right\"";
    case 2:
        return " align=\"center\"";
    case 3:
        return " align=\"justify\"";
    default:
        return "";
    }
}

bool SsrDocument::IsListMarker(const std::string& marker)
{
    return marker == "•" || marker == "●" || marker == "♦" || marker == "■";
}

const Json::Value& SsrDocument::Child(const Json::Value& element, unsigned index)
{
    const Json::Value& children = element["elements"];
    if (!children.isArray() || index >= children.size())
        return empty_value;
    return children[index];
}

//SsrPage

std::string SsrPage::MakePage(const std::string& index_html, const std::string& title, const std::string& description, const std::string& lang,
    const std::string& article)
{
    if (index_html.empty() || article.empty())
        return "";

    //the head is modified before the article is injected: the document content is user text and must not be scanned for head markup
    std::string page = index_html;

    if (!description.empty())
    {
        //remove the existing meta description tags: scan the meta tags themselves, not the word "description" - it may occur in the page text
        size_t tag_begin = page.find("<meta");
        while (tag_begin != std::string::npos)
        {
            size_t tag_end = page.find(">", tag_begin);
            if (tag_end == std::string::npos)
                break;
            if (page.substr(tag_begin, tag_end - tag_begin).find("description") != std::string::npos)
            {
                page.erase(tag_begin, tag_end - tag_begin + 1);
                tag_begin = page.find("<meta", tag_begin);
            }
            else
                tag_begin = page.find("<meta", tag_end);
        }

        std::string meta_description = "<meta name=\"description\" content=\"" + SsrDocument::EscapeHtml(description) + "\">";
        size_t title_end = page.find("</title>");
        if (title_end != std::string::npos)
            page.insert(title_end + 8, meta_description);
        else
        {
            size_t head = page.find("<head>");
            if (head != std::string::npos)
                page.insert(head + 6, meta_description);
        }
    }

    if (!title.empty())
    {
        size_t begin = page.find("<title");
        if (begin != std::string::npos)
        {
            begin = page.find(">", begin);
            size_t end = page.find("</title>", begin);
            if (begin != std::string::npos && end != std::string::npos)
                page.replace(begin + 1, end - begin - 1, SsrDocument::EscapeHtml(title));
        }
    }

    if (!lang.empty())
    {
        size_t html_begin = page.find("<html");
        size_t tag_end = page.find(">", html_begin);
        if (html_begin != std::string::npos && tag_end != std::string::npos &&
            page.substr(html_begin, tag_end - html_begin).find("lang") == std::string::npos)
        {
            page.insert(tag_end, " lang=\"" + lang + "\"");
        }
    }

    //the article becomes the fallback content of the canvas: browsers do not display it, the SPA replaces the container on mount, crawlers read it
    std::string container;
    if (page.find("<div id=q-app></div>") != std::string::npos)
        container = "<div id=q-app></div>";
    else if (page.find("<div id=\"q-app\"></div>") != std::string::npos)
        container = "<div id=\"q-app\"></div>";
    else
        return "";

    size_t pos = page.find(container);
    std::string replacement = container.substr(0, container.size() - 6) + "<canvas tabindex=\"-1\">" + article + "</canvas></div>";
    page.replace(pos, container.size(), replacement);

    return page;
}

//SsrCache

SsrCache& SsrCache::GetInstance()
{
    static SsrCache cache;
    return cache;
}

bool SsrCache::GetIndexPage(std::string& index_html)
{
    std::string root = drogon::app().getDocumentRoot();
    if (root.empty() || root.back() != '/')
        root += "/";
    fs::path path = fs::path(root) / drogon::app().getHomePage();

    std::error_code ec;
    FileTime mtime = fs::last_write_time(path, ec);
    if (ec)
        return false;

    std::lock_guard<std::mutex> lock(cache_mutex);
    if (!index_page.empty() && index_mtime == mtime)
    {
        index_html = index_page;
        return true;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return false;
    index_page.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    index_mtime = mtime;
    index_html = index_page;
    return true;
}

//Returns the cached page of a library document or generates it
bool SsrCache::GetLibraryPage(const std::string& path, const std::string& title, std::string& page_html)
{
    std::error_code ec;
    FileTime mtime = fs::last_write_time(path, ec);
    if (ec)
        return false;

    std::string index_html;
    if (!GetIndexPage(index_html))
        return false;

    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = library_pages.find(path);
        if (it != library_pages.end() && it->second.mtime == mtime)
        {
            page_html = it->second.page;
            return true;
        }
    }

    //the include_documents are not expanded: the SSR article is built from the document itself
    Json::Value document;
    if (!SsrDocument::ReadLibraryFile(path, document))
        return false;

    SsrDocument ssr(document);
    std::string description = ssr.FirstParagraphText(160);
    std::string lang = SsrDocument::LangFromLanguage(ssr.Language());
    std::string page = SsrPage::MakePage(index_html, title, description, lang, ssr.GenerateArticle(title));
    if (page.empty())
        return false;

    std::lock_guard<std::mutex> lock(cache_mutex);
    library_pages[path] = {mtime, std::move(page)};
    page_html = library_pages[path].page;
    return true;
}

}
