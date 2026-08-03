/*
 * Yutovo Server
 * Copyright (C) 2022-2026 Yutovo developers. All rights reserved.
 * This file is a part of the Yutovo project
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include "service_controller.h"
#include "../logic/clear_db.h"
#include <functional>
#include <fstream>
#include <iostream>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/device/array.hpp>
#include <drogon/HttpAppFramework.h>
#include <drogon/plugins/RealIpResolver.h>

namespace yutovo_server
{

namespace fs = std::filesystem;
using namespace std::chrono_literals;

//ServiceController

ServiceController::ServiceController()
{
#ifdef REMOTE_SOLVER
    const Json::Value& v = app().getCustomConfig();
    std::string solver_address = v.get("solver_address", "").asString();
    if (solver_address.empty())
        throw std::system_error(ENOTDIR, std::generic_category(), "Solver address not defined");

    solver_client = WebSocketClient::newWebSocketClient(solver_address);
    solver_client->setMessageHandler(
        [&](const std::string& message, const WebSocketClientPtr&, const WebSocketMessageType& type)
        {
            if (type == WebSocketMessageType::Text)
                solver_response = message;
        });
    
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/");
    solver_client->connectToServer(req, 
        [&](ReqResult r, const HttpResponsePtr&, const WebSocketClientPtr&)
        {
            if (r != ReqResult::Ok)
            {
                GetLogger("")->Error("ServiceController not connected to Solver");
                return;
            }
            GetLogger("")->Info("ServiceController connected to Solver");
            solver_client->getConnection()->setPingMessage("", 2s);
        });
#endif
}

void ServiceController::GetLibraryDocuments(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("GetLibraryDocuments request");

    auto json_object = req->getJsonObject();
    std::string language = "en";
    if (json_object && json_object->isObject() && json_object->isMember("language") && (*json_object)["language"].isString())
        language = (*json_object)["language"].asString();

    std::function<void (const fs::path& path, const std::string& name, Json::Value& json)> get_files = 
        [&](const fs::path& path, const std::string& name, Json::Value& json)
        {
            //if the order file exists, read the files order from it
            std::vector<std::string> order;
            if (fs::exists(path / ".order"))
            {
                std::string order_file = path / ".order";
                try
                {
                    std::ifstream file(order_file);
                    if (!file.is_open())
                    {
                        GetLogger(session_id)->Error("File not open '{}'", order_file);
                    }
                    else
                    {
                        std::string line;
                        while (std::getline(file, line))
                            order.push_back(line);
                    }
                }
                catch (const std::ios_base::failure& ex)
                {
                    GetLogger(session_id)->Error("Error opening file '{}': {}", order_file, ex.what());
                }
            }

            std::vector<fs::path> sorted_dirs;
            std::vector<fs::path> sorted_files;
            for (const auto& entry : fs::directory_iterator(path))
            {
                if (entry.is_directory())
                {
                    int pos = -1;
                    if (!order.empty())
                    {
                        auto s = entry.path().filename().c_str();
                        auto it = std::find(order.begin(), order.end(), entry.path().filename().c_str());
                        if (it != order.end())
                            pos = std::distance(order.begin(), it);
                        if (pos == -1)
                            sorted_dirs.push_back(entry.path());
                        else
                        {
                            if (pos < sorted_dirs.size())
                            {
                                sorted_dirs[pos] = entry.path();
                            }
                            else
                            {
                                for (int i = sorted_dirs.size(); i < pos; ++i)
                                    sorted_dirs.push_back(fs::path());
                                sorted_dirs.push_back(entry.path());
                            }
                        }
                    }
                    else
                        sorted_dirs.push_back(entry.path());
                }
                else if (entry.is_regular_file())
                {
                    if (entry.path().stem() != ".order")
                    {
                        int pos = -1;
                        if (!order.empty())
                        {
                            auto it = std::find(order.begin(), order.end(), entry.path().filename().c_str());
                            if (it != order.end())
                                pos = std::distance(order.begin(), it);
                            if (pos == -1)
                                sorted_files.push_back(entry.path());
                            else
                            {
                                if (pos < sorted_files.size())
                                {
                                    sorted_files[pos] = entry.path();
                                }
                                else
                                {
                                    for (int i = sorted_files.size(); i < pos; ++i)
                                        sorted_files.push_back(fs::path());
                                    sorted_files.push_back(entry.path());
                                }
                            }
                        }
                        else
                            sorted_files.push_back(entry.path());
                    }
                }
            }

            if (order.empty())
            {
                std::sort(sorted_dirs.begin(), sorted_dirs.end());
                std::sort(sorted_files.begin(), sorted_files.end());
            }

            json["name"] = name;

            Json::Value dirs(Json::arrayValue);
            for (const auto& entry : sorted_dirs)
            {
                if (entry.empty())
                    continue;
                Json::Value dir(Json::objectValue);
                get_files(entry, entry.stem(), dir);
                dirs.append(dir);
            }
            json["dirs"] = dirs;

            Json::Value files(Json::arrayValue);
            for (const auto& entry : sorted_files)
            {
                if (entry.empty())
                    continue;
                files.append(entry.filename().c_str());
            }
            json["files"] = files;
        };
    
    Json::Value root;
    get_files(fs::path(library_path + "/" + language), "library", root);

    SendJson(callback, root);
}

void ServiceController::LoadLibraryDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("LoadLibraryDocument error: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    std::string language = "en";
    if (json->isMember("language") && (*json)["language"].isString())
        language = (*json)["language"].asString();

    std::string document;
    if (!json->isMember("document") || !(*json)["document"].isString())
    {
        GetLogger(session_id)->Error("LoadLibraryDocument error: Wrong request: empty document");
        SendError(k400BadRequest, "Wrong request: empty document", session_id, callback);
        return;
    }
    document = (*json)["document"].asString();
    
    GetLogger(session_id)->Info("LoadLibraryDocument request document={}", "/" + language + document);
    document = library_path + language + document;

    SendLibraryDocument(req, document, callback);
}

void ServiceController::SaveLibraryDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("SaveLibraryDocument error: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    std::string language = "en";
    if (json->isMember("language") && (*json)["language"].isString())
        language = (*json)["language"].asString();

    std::string document;
    if (!json->isMember("document") || !(*json)["document"].isString())
    {
        GetLogger(session_id)->Error("SaveLibraryDocument error: Wrong request: empty document");
        SendError(k400BadRequest, "Wrong request: empty document", session_id, callback);
        return;
    }
    document = (*json)["document"].asString();
    size_t p = document.find_last_of("/");
    if (p == std::string::npos || p >= document.length())
    {
        GetLogger(session_id)->Error("SaveLibraryDocument error: Wrong document name");
        SendError(k400BadRequest, "Wrong document name", session_id, callback);
        return;
    }
    
    std::string name = document.substr(p + 1);
    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
        user_id = "-1";

    std::string document_id;

    GetLogger(session_id)->Debug("SaveLibraryDocument request: document={}", document);

    ClearDbTurnOff t; //skip the clear db circles

    try
    {
        if (!AddDocument(req, user_id, document_id, name, 0, callback))
            return;

        if (json->isMember("json"))
        {
            //save this json
            auto d = (*json)["json"].asString();

            orm::Result result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", d, document_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error inserting a document");
                SendError(k500InternalServerError, "Error inserting a document", session_id, callback);
                return;
            }
        }
        else
        {
            //save from the file
            document = library_path + language + document;
            fs::path path;
            Json::Value doc;

            //check if the path is inside library_path
            try
            {
                path = fs::canonical(fs::path(document));
                if (!std::string(path.c_str()).starts_with(library_path))
                {
                    GetLogger(session_id)->Error("SaveLibraryDocument error: Path not found: {}", path.c_str());
                    SendError(k404NotFound, "Path not found", session_id, callback);
                    return;
                }

                std::ifstream f(path.string().c_str());
                f >> doc;
            }
            catch (const std::exception& ex)
            {
                SendError(k404NotFound, "Path not found", session_id, callback);
                return;
            }

            orm::Result result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", JsonToString(doc), document_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error inserting a document");
                SendError(k500InternalServerError, "Error inserting a document", session_id, callback);
                return;
            }
        }

        db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);
        SendOkDocumentName(callback, document_id, session_id, name);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

#ifdef REMOTE_SOLVER
void ServiceController::ListIdentifiers(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    if (!solver_client || !solver_client->getConnection())
    {
        GetLogger(session_id)->Error("ListIdentifiers error: Solver socket not open");
        SendError(k500InternalServerError, "Solver socket not open", session_id, callback);
        return;
    }
    
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("ListIdentifiers error: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    (*json)["command"] = "LIST_IDENTIFIERS";
    Json::FastWriter json_writer;
    solver_response = "";
    solver_client->getConnection()->send(json_writer.write(*json));

    time_t now = time(nullptr);
    //wait for response from the service
    while (solver_response.empty())
    {
        if (time(nullptr) - now >= solver_timeout)
        {
            GetLogger(session_id)->Error("ListIdentifiers error: Solver not response");
            SendError(k500InternalServerError, "Solver not response", session_id, callback);
            return;
        }
        std::this_thread::sleep_for(1ms);
    }
    
    SendJson(callback, solver_response, session_id);
}
#endif

void ServiceController::NewDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Info("NewDocument request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("NewDocument error: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }
    if (session_id.empty())
    {
        GetLogger(session_id)->Error("NewDocument error: Wrong request: empty session_id");
        SendError(k400BadRequest, "Wrong request: empty session_id", session_id, callback);
        return;
    }

    bool add_doc = false;
    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
        user_id = "-1";
    std::string document_id = session->get<std::string>("document_id");
    GetLogger(session_id)->Info("New document: document_id={}", document_id);

    ClearDbTurnOff t; //skip the clear db circles for a while

    try
    {
        if (user_id == "-1")
        {
            //for unregistered user reset his document to empty
            db->execSqlSync("update user_documents set document='{}' where document_id=$1", document_id);
            SendOk(callback, document_id, session_id);
        }
        else
        {
            std::string name;
            if (json->isMember("name") && (*json)["name"].isString())
                name = (*json)["name"].asString();
            int language = 0;
            if (json->isMember("language") && (*json)["language"].isInt())
                language = (*json)["language"].asInt();
            
            //for registered user create a new document
            add_doc = true;
            if (!AddDocument(req, user_id, document_id, name, language, callback))
                return;

            int max_file_size = session->get<int>("max_file_size");
            Json::Value text;
            Json::Value& doc = *json;
            if (doc.isObject() && doc.isMember("text"))
            {
                auto d = JsonToString(doc);
                if (d.size() > max_file_size)
                {
                    db->execSqlSync("delete from user_documents where document_id=$1", document_id);
                    GetLogger(session_id)->Error("Document size is more then limit");
                    SendError(k400BadRequest, "Document size is more then limit", session_id, callback);
                    return;
                }
    
                auto result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", d, document_id);
                if (result.affectedRows() == 0)
                {
                    db->execSqlSync("delete from user_documents where document_id=$1", document_id);
                    GetLogger(session_id)->Error("Database error: Error inserting a document: document_id={}", document_id);
                    SendError(k500InternalServerError, "Error inserting a document", session_id, callback);
                    return;
                }
            }

            if (json->isMember("json"))
            {
                auto d = JsonToString((*json)["json"]);
                if (d.size() > max_file_size)
                {
                    db->execSqlSync("delete from user_documents where document_id=$1", document_id);
                    GetLogger(session_id)->Error("Document size is more then limit");
                    SendError(k400BadRequest, "Document size is more then limit", session_id, callback);
                    return;
                }

                orm::Result result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", 
                    (*json)["json"].isString() ? (*json)["json"].asString() : d, document_id);
                if (result.affectedRows() == 0)
                {
                    db->execSqlSync("delete from user_documents where document_id=$1", document_id);
                    GetLogger(session_id)->Error("Database error: Error inserting a document: document_id={}", document_id);
                    SendError(k500InternalServerError, "Error inserting a document", session_id, callback);
                    return;
                }
            }

            db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);

            SendOkDocumentName(callback, document_id, session_id, name);
        }
    }
    catch (const orm::DrogonDbException& e)
    {
        if (add_doc)
            db->execSqlSync("delete from user_documents where document_id=$1", document_id);
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, "Error inserting a document", session_id, callback);
    }
}

void ServiceController::SaveDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("SaveDocument request");
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("SaveDocument error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }
    if (session_id.empty())
    {
        GetLogger(session_id)->Error("SaveDocument error: Wrong request: empty session_id");
        SendError(k400BadRequest, "Wrong request: empty session_id", session_id, callback);
        return;
    }

    Json::Value& doc = *json;
    if (!doc.isMember("text"))
    {
        GetLogger(session_id)->Error("SaveDocument error: Text field not found in the request");
        SendError(k400BadRequest, "Text field not found in the request", session_id, callback);
        return;
    }

    Json::Value& text = doc["text"];
    orm::DbClientPtr db = app().getDbClient();

    if (!text.isMember("id") || text["id"].asString().empty())
    {
        GetLogger(session_id)->Error("SaveDocument error: Wrong request: empty id");
        SendError(k400BadRequest, "Wrong request: empty id", session_id, callback);
        return;
    }

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
        user_id = "-1";
    std::string document_id = "-1";
    int max_file_size = session->get<int>("max_file_size");
    int document_size = 0;

    try
    {
        orm::Result result = db->execSqlSync("select document_id from user_sessions where session_id=$1", session_id);
        if (result.size() == 0)
        {
            GetLogger(session_id)->Error("Database error: document not found");
            SendError(k500InternalServerError, "Document not found", session_id, callback);
            return;
        }

        auto row = result[0];
        document_id = row["document_id"].as<std::string>();

        if (document_id != "-1")
        {
            result = db->execSqlSync("select user_id, shared, pg_column_size(document) from user_documents where document_id=$1", document_id);
            if (result.size() == 0)
            {
                GetLogger(session_id)->Error("Database error: document not found");
                SendError(k500InternalServerError, "Document not found", session_id, callback);
                return;
            }

            auto row = result[0];
            if (row["user_id"].as<std::string>() != user_id && !row["shared"].as<bool>())
            {
                //saving this foreign document is prohibited
                GetLogger(session_id)->Error("SaveDocument error: Document saving is prohibited: document_id={}", document_id);
                SendError(k403Forbidden, "Document saving is prohibited", session_id, callback);
                return;
            }
            document_size = row["pg_column_size"].as<int>();
        }
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
        return;
    }

    auto id = text["id"].asString();
    if (id == "0")
    {
        //update whole document
        try
        {
            auto document = JsonToString(doc);
            if (document.size() > max_file_size)
            {
                GetLogger(session_id)->Error("Document size is more then limit");
                SendError(k400BadRequest, "Document size is more then limit", session_id, callback);
                return;
            }

            std::string name;
            if (document_id == "-1")
            {
                //insert new document
                if (!AddDocument(req, user_id, document_id, name, 0, callback))
                    return;
            }

            orm::Result result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", document, document_id);
            if (result.affectedRows() == 0)
            {
                if (!AddDocument(req, user_id, document_id, name, 0, callback))
                    return;
            }

            db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);
            SendOkDocumentName(callback, document_id, session_id, name);
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), session_id, callback);
        }
        return;
    }

    //update a part of the document
    try
    {
        if (document_id == "-1")
        {
            GetLogger(session_id)->Error("Wrong document id for session_id={}", session_id);
            SendError(k400BadRequest, "Wrong document id", session_id, callback);
            return;
        }

        //parse the string id
        std::vector<int> _id;
        if (!ParseId(id, _id))
        {
            SendError(k400BadRequest, "Wrong Id", session_id, callback);
            return;
        }

        std::string path = "{\"text\"";
        for (size_t i = 1; i < _id.size(); ++i)
            path += ",\"elements\"," + std::to_string(_id[i]);
        path += "}";

        auto document = JsonToString(text);

        //firstly check the size
        if (document_size + document.size() > max_file_size)
        {
            GetLogger(session_id)->Error("Document size is more then limit");
            SendError(k400BadRequest, "Document size is more then limit", session_id, callback);
            return;
        }

        orm::Result result = db->execSqlSync("update user_documents set document=jsonb_set(document,$1::text[], $2::jsonb) "
            "where document_id=$3", path, document, document_id);
        result = db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);
        SendOk(callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void ServiceController::SaveAsDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("SaveAsDocument request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("SaveAsDocument error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    Json::Value& doc = *json;
    std::string document_id;
    if (json->isMember("document_id") && ((*json)["document_id"].isInt() || (*json)["document_id"].isString()))
        document_id = (*json)["document_id"].asString();
    if (document_id.empty() || document_id == "-1")
    {
        GetLogger(session_id)->Error("SaveAsDocument error: Wrong request: document_id field not found in the request");
        SendError(k400BadRequest, "document_id field not found in the request", session_id, callback);
        return;
    }

    std::string name;
    if (json->isMember("name") || (*json)["name"].isString())
        name = (*json)["name"].asString();
    if (name.empty())
    {
        GetLogger(session_id)->Error("Empty name field");
        SendError(k400BadRequest, "name field not found in the request", session_id, callback);
        return;
    }

    GetLogger(session_id)->Info("Save document: name: {}", name);

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
    {
        GetLogger(session_id)->Error("SaveAsDocument error: Wrong user_id");
        SendError(k400BadRequest, "Wrong user_id", session_id, callback);
        return;
    }

    orm::DbClientPtr db = app().getDbClient();
    ClearDbTurnOff t; //skip the clear db circles

    try
    {
        orm::Result result = db->execSqlSync("select user_id, document from user_documents where document_id=$1", document_id);
        if (result.size() == 0)
        {
            GetLogger(session_id)->Error("Database error: document not found");
            SendError(k500InternalServerError, "Document not found", session_id, callback);
            return;
        }

        auto row = result[0];
        
        result = db->execSqlSync("select 1 from user_documents where user_id=$1 and name=$2", user_id, name); //check the document name is unique
        if (result.size() > 0)
        {
            GetLogger(session_id)->Error("Database error: document with such name already exists");
            SendError(k409Conflict, "Document with such name already exists", session_id, callback);
            return;
        }

        auto doc = row["document"].as<std::string>();

        if (!AddDocument(req, user_id, document_id, name, 0, callback))
            return;

        result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", doc, document_id);
        if (result.affectedRows() == 0)
        {
            GetLogger(session_id)->Error("Database error: Error inserting a document");
            SendError(k500InternalServerError, "Error inserting a document", session_id, callback);
            return;
        }

        SendOk(callback, document_id, session_id);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void ServiceController::LoadDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("LoadDocument request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("LoadDocument error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    SessionPtr session = req->session();
    std::string document_id;
    if (json->isMember("document_id") && ((*json)["document_id"].isInt() || (*json)["document_id"].isString()))
        document_id = (*json)["document_id"].asString();
    if (document_id.empty() || document_id == "-1")
        document_id = req->getCookie("document_id");
    if (document_id.empty() || document_id == "-1")
        session->get<std::string>("document_id");
    if (document_id.empty())
    {
        GetLogger(session_id)->Error("LoadDocument error: Wrong request: empty document_id");
        SendError(k400BadRequest, "Wrong request: empty document_id", session_id, callback);
        return;
    }

    GetLogger(session_id)->Info("Load document: document_id={}", document_id);

    std::string id;
    if (json->isObject() && json->isMember("id") && (*json)["id"].isString())
        id = (*json)["id"].asString();
    
    auto user_id = session->get<std::string>("user_id");

    ClearDbTurnOff t; //skip the clear db circles for a while

    orm::DbClientPtr db = app().getDbClient();

    try
    {
        //check this user can load this document
        orm::Result result = db->execSqlSync("select user_id, public from user_documents where document_id=$1", document_id);
        if (result.size() == 0)
        {
            GetLogger(session_id)->Error("LoadDocument error: No such document: document_id={}", document_id);
            SendError(k404NotFound, "No such document", session_id, callback);
            return;
        }

        auto row = result[0];
        if (row["user_id"].as<std::string>() != user_id)
        {
            if (!row["public"].as<bool>())
            {
                GetLogger(session_id)->Error("LoadDocument error: This document is not public: document_id={}", document_id);
                SendError(k403Forbidden, "This document is not public", session_id, callback);
                return;
            }
        }

        if (id.empty())
        {
            //get the whole document
            result = db->execSqlSync("select document from user_documents where document_id=$1", document_id);
            auto row = result[0];
            SendJson(callback, row["document"].as<std::string>(), session_id);
        }
        else
        {
            //get a part of the document
            std::vector<int> _id;
            if (!ParseId(id, _id))
            {
                GetLogger(session_id)->Error("LoadDocument error: Wrong Id: document_id={}", document_id);
                SendError(k400BadRequest, "Wrong Id", session_id, callback);
                return;
            }

            std::string path = "'text'";
            for (size_t i = 1; i < _id.size(); ++i)
                path += "->'elements'->" + std::to_string(_id[i]);

            result = db->execSqlSync("select document->" + path + " as document from user_documents where document_id=$1", document_id);
            if (result.size() == 0)
            {
                GetLogger(session_id)->Error("LoadDocument error: No such session: document_id={}", document_id);
                SendError(k400BadRequest, "No such session", session_id, callback);
                return;
            }

            auto row = result[0];
            SendJson(callback, row["document"].as<std::string>(), session_id);
        }

        if (!session_id.empty())
        {
            db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);
            if (!user_id.empty())
                db->execSqlSync("update users set document_id=$1 where user_id=$2", document_id, user_id);
        }
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void ServiceController::LoadIncludeDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("LoadIncludeDocument request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("LoadIncludeDocument error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    std::string language = "en";
    if (json->isMember("language") && (*json)["language"].isString())
        language = (*json)["language"].asString();

    std::string name;
    if (!json->isMember("name") || !(*json)["name"].isString())
    {
        GetLogger(session_id)->Error("LoadLibraryDocument error: Wrong request: empty document name");
        SendError(k400BadRequest, "Wrong request: empty document name", session_id, callback);
        return;
    }
    name = (*json)["name"].asString();

    std::string current_document;
    if (json->isMember("current_document") && (*json)["current_document"].isString())
        current_document = (*json)["current_document"].asString();

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");

    GetLogger(session_id)->Info("LoadIncludeDocument request name={}", name);

    ClearDbTurnOff t; //skip the clear db circles for a while

    orm::DbClientPtr db = app().getDbClient();

    if (!user_id.empty())
    {
        //include document may be user document
        try
        {
            //find document by name
            orm::Result result = db->execSqlSync("select document from user_documents where user_id=$1 and name=$2", user_id, name);
            if (result.size() != 0)
            {
                auto row = result[0];
                SendJson(callback, row["document"].as<std::string>(), session_id);
                return;
            }
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), session_id, callback);
            return;
        }
    }

    //otherwise find document in the library
    std::string path;
    try
    {
        fs::path p;
        if (name.starts_with('/'))
        {
            p = fs::path(library_path + language);
            name.erase(name.begin());
        }
        else
        {
            p = fs::path(library_path + language + current_document);
            if (!current_document.empty())
                p = p.parent_path();
        }
        p /= name;
        path = fs::canonical(p);
    }
    catch (const std::exception& ex)
    {
        GetLogger(session_id)->Error("GetIncludeDocument error: Path not found: {}", name);
        SendError(k404NotFound, "Path not found", session_id, callback);
        return;
    }

    if (!fs::exists(path))
    {
        GetLogger(session_id)->Error("GetIncludeDocument error: No such document: {}", name);
        SendError(k404NotFound, "No such document", session_id, callback);
        return;
    }

    SendLibraryDocument(req, path.c_str(), callback);
}

void ServiceController::DeleteDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("DeleteDocument request");

    auto json = req->getJsonObject();
    std::string document_id;
    if (json && json->isObject() && json->isMember("document_id") && (*json)["document_id"].isInt())
        document_id = (*json)["document_id"].asString();
    else
        document_id = req->getCookie("document_id");
    if (document_id.empty())
    {
        GetLogger(session_id)->Error("DeleteDocument error: Wrong request: empty document_id");
        SendError(k400BadRequest, "Wrong request: empty document_id", session_id, callback);
        return;
    }

    GetLogger(session_id)->Info("Delete document: document_id={}", document_id);

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (session_id.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty session_id", session_id, callback);
        return;
    }

    ClearDbTurnOff t; //skip the clear db circles

    orm::DbClientPtr db = app().getDbClient();

    try
    {
        //check this user can delete this document
        orm::Result result = db->execSqlSync("select 1 from user_documents where document_id=$1 and user_id=$2", document_id, user_id);
        if (result.size() == 0)
        {
            GetLogger(session_id)->Error("DeleteDocument error: Cannot delete document: document_id={}", document_id);
            SendError(k403Forbidden, "Cannot delete document", session_id, callback);
            return;
        }

        db->execSqlSync("delete from user_documents where document_id=$1", document_id);
        db->execSqlSync("update user_sessions set document_id=-1 where session_id=$1", session_id);
        SendOk(callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void ServiceController::ListDocuments(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("ListDocuments request");

    ClearDbTurnOff t; //skip the clear db circles for a while

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");

    try
    {
        orm::Result result = db->execSqlSync("select document_id, name from user_documents where user_id=$1", user_id);

        Json::Value root(Json::arrayValue);
        for (int i = 0; i < result.size(); ++i)
        {
            Json::Value v(Json::objectValue);
            auto row = result[i];
            v["id"] = row["document_id"].as<int>();
            v["name"] = row["name"].as<std::string>();
            root.append(v);
        }

        SendJson(callback, root);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void ServiceController::GetDocumentId(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("GetDocumentId request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("GetDocumentId error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    ClearDbTurnOff t; //skip the clear db circles for a while
    orm::DbClientPtr db = app().getDbClient();

    std::string document_id;
    if (!json->isMember("name") || !(*json)["name"].isString())
    {
        GetLogger(session_id)->Error("GetDocumentId error: Wrong request: name not found in the request");
        SendError(k400BadRequest, "Name not found in the request", session_id, callback);
        return;
    }

    //find document by name
    std::string name = (*json)["name"].asString();
    if (!name.empty())
    {
        try
        {
            orm::Result result = db->execSqlSync("select document_id from user_documents where name=$1", name);
            if (result.size() == 0)
            {
                GetLogger(session_id)->Error("LoadDocument error: No such document: {}", name);
                SendError(k404NotFound, "No such document", session_id, callback);
                return;
            }
    
            auto row = result[0];
            Json::Value v(Json::objectValue);
            v["document_id"] = row["document_id"].as<int>();
            GetLogger(session_id)->Debug("DocumentId id={}", row["document_id"].as<std::string>());
            SendJson(callback, v);
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), session_id, callback);
        }
    }
}

void ServiceController::GetDocumentName(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("GetDocumentName request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("GetDocumentName error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    std::string document_id, name;
    if (json->isMember("document_id") && ((*json)["document_id"].isInt() || (*json)["document_id"].isString()))
        document_id = (*json)["document_id"].asString();
    else if (json->isMember("name") && (*json)["name"].isString())
        name = (*json)["name"].asString();
    else
        document_id = req->getCookie("document_id"); //this request is for current document
    if (document_id.empty() && name.empty())
    {
        GetLogger(session_id)->Error("GetDocumentName error: Wrong request: empty document_id or name");
        SendError(k400BadRequest, "Wrong request: empty document_id or name", session_id, callback);
        return;
    }

    if (!name.empty())
    {
        std::string lang = "en";
        if (json->isMember("lang") && (*json)["lang"].isString())
            lang = (*json)["lang"].asString();
        std::string document = library_path + lang + name;
        GetLogger(session_id)->Info("Get document name: name={}, lang={}", name, lang);

        fs::path path;
        try
        {
            path = fs::canonical(fs::path(document));
        }
        catch (const std::exception& ex)
        {
            GetLogger(session_id)->Error("GetDocumentName error: Path not found: {}", document);
            SendError(k404NotFound, "Path not found", session_id, callback);
            return;
        }
    
        if (!fs::exists(path))
        {
            GetLogger(session_id)->Error("GetDocumentName error: No such document: {}", name);
            SendError(k404NotFound, "No such document", session_id, callback);
            return;
        }

        std::string p = path.string();
        p = p.substr(library_path.length() + lang.length());

        Json::Value v(Json::objectValue);
        v["name"] = p;
        GetLogger(session_id)->Debug("Document name: name={}", p);
        SendJson(callback, v);
        return;
    }

    GetLogger(session_id)->Info("Get document name: document_id={}", document_id);

    ClearDbTurnOff t; //skip the clear db circles for a while

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();

    try
    {
        orm::Result result = db->execSqlSync("select user_id, name, public from user_documents where document_id=$1", document_id);
        if (result.size() == 0)
        {
            GetLogger(session_id)->Error("GetDocumentName error: No such document: {}", document_id);
            SendError(k404NotFound, "No such document", session_id, callback);
            return;
        }

        auto row = result[0];
        if (row["user_id"].as<std::string>() != session->get<std::string>("user_id"))
        {
            if (!row["public"].as<bool>())
            {
                GetLogger(session_id)->Error("GetDocumentName error: This document is not public: {}", document_id);
                SendError(k403Forbidden, "This document is not public", session_id, callback);
                return;
            }
        }

        Json::Value v(Json::objectValue);
        v["name"] = row["name"].as<std::string>();
        GetLogger(session_id)->Debug("Document name: name={}", row["name"].as<std::string>());
        SendJson(callback, v);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void ServiceController::RenameDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("RenameDocument request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("RenameDocument error: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    std::string document_id;
    if (json->isMember("document_id") && ((*json)["document_id"].isInt()) || (*json)["document_id"].isString())
        document_id = (*json)["document_id"].asString();
    else
        document_id = req->getCookie("document_id"); //this request is for current document
    if (document_id.empty())
    {
        GetLogger(session_id)->Error("RenameDocument error: Wrong request: empty document_id");
        SendError(k400BadRequest, "Wrong request: empty document_id", session_id, callback);
        return;
    }

    std::string name;
    if (json->isMember("name") && (*json)["name"].isString())
        name = (*json)["name"].asString();
    if (name.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty name", session_id, callback);
        return;
    }

    GetLogger(session_id)->Info("Rename document: document_id={}", document_id);

    ClearDbTurnOff t; //skip the clear db circles for a while

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();

    try
    {
        orm::Result result = db->execSqlSync("update user_documents set name=$1 where document_id=$2", name, document_id);
        if (result.affectedRows() == 0)
        {
            GetLogger(session_id)->Error("Database error: Error updaing a document");
            SendError(k500InternalServerError, "Error updating a document", session_id, callback);
            return;
        }

        SendOk(callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void ServiceController::SetUserSettings(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("SetUserSettings request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("SetUserSettings error: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
    {
        GetLogger(session_id)->Error("SetUserSettings error: Empty user_id");
        SendError(k500InternalServerError, "Empty user_id", session_id, callback);
        return;
    }

    std::string settings, name, email, password, old_password, captcha, email_code;
    if (json->isMember("settings") && (*json)["settings"].isString())
        settings = (*json)["settings"].asString();
    if (json->isMember("name") && (*json)["name"].isString())
        name = (*json)["name"].asString();
    if (json->isMember("email") && (*json)["email"].isString())
        email = (*json)["email"].asString();
    if (json->isMember("password") && (*json)["password"].isString())
        password = (*json)["password"].asString();
    if (json->isMember("old_password") && (*json)["old_password"].isString())
        old_password = (*json)["old_password"].asString();
    if (json->isMember("captcha") && (*json)["captcha"].isString())
        captcha = (*json)["captcha"].asString();
    if (json->isMember("email_code") && (*json)["email_code"].isString())
        email_code = (*json)["email_code"].asString();

    if (settings.empty() && name.empty() && email.empty() && (password.empty() || old_password.empty()))
    {
        GetLogger(session_id)->Error("SetUserSettings error: Wrong request: empty request");
        SendError(k400BadRequest, "Wrong request: empty request", session_id, callback);
        return;
    }

    ClearDbTurnOff t; //skip the clear db circles for a while

    if (!settings.empty()) //set settings
    {
        Json::Reader reader;
        Json::Value settings_val;
        if (!reader.parse(settings, settings_val))
        {
            GetLogger(session_id)->Error("Database error: Error updating settings");
            SendError(k500InternalServerError, "Error updating settings", session_id, callback);
            return;
        }

        try
        {
            //update the existing json, load it, change and save
            orm::Result result = db->execSqlSync("select settings from users where user_id=$1", user_id);
            if (result.size() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error updating settings");
                SendError(k500InternalServerError, "Error updating settings", session_id, callback);
                return;
            }
    
            Json::Value s_val;
            auto row = result[0];
            std::string s = row["settings"].as<std::string>();
            if (s.empty())
            {
                s_val = settings_val;
            }
            else
            {
                if (!reader.parse(s, s_val))
                {
                    GetLogger(session_id)->Error("Database error: Error updating settings");
                    SendError(k500InternalServerError, "Error updating settings", session_id, callback);
                    return;
                }
    
                std::function<void (Json::Value&, Json::Value&)> merge =
                    [&](Json::Value& a, Json::Value& b)
                    {
                        if (!a.isObject() || !b.isObject())
                            return;
                        for (const auto& key : b.getMemberNames())
                        {
                            if (a[key].isObject())
                                merge(a[key], b[key]);
                            else
                                a[key] = b[key];
                        }
                    };
                
                merge(s_val, settings_val);
            }

            result = db->execSqlSync("update users set settings=$1 where user_id=$2", JsonToString(s_val), user_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error updating settings");
                SendError(k500InternalServerError, "Error updating settings", session_id, callback);
                return;
            }
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), session_id, callback);
            return;
        }
    }

    if (!name.empty()) //set name
    {
        try
        {
            orm::Result result = db->execSqlSync("update users set name=$1 where user_id=$2", name, user_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error updating name");
                SendError(k500InternalServerError, "Error updating name", session_id, callback);
                return;
            }
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), session_id, callback);
            return;
        }
    }

    if (!email.empty()) //set e-mail
    {
        try
        {
            //check the e-mail doesn't exist
            orm::Result result = db->execSqlSync("select 1 from users where email=$1", email);
            if (result.size() != 0)
            {
                GetLogger(session_id)->Error("e-mail already exists: {}", email);
                SendError(k409Conflict, "e-mail already exists", session_id, callback);
                return;
            }

            result = db->execSqlSync("update users set email=$1 where user_id=$2", email, user_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error updating email");
                SendError(k500InternalServerError, "Error updating email", session_id, callback);
                return;
            }
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), session_id, callback);
            return;
        }
    }

    if (!password.empty() && !old_password.empty()) //set new password
    {
#ifndef TEST
        auto captcha = (*json)["captcha"].asString();
        if (captcha.empty() || session->get<std::string>("captcha") != captcha)
        {
            SendError(k400BadRequest, "Wrong captcha", session_id, callback);
            return;
        }
        auto email_code = (*json)["email_code"].asString();
        if (email_code.empty() || session->get<std::string>("email_code") != email_code)
        {
            SendError(k400BadRequest, "Wrong email code", session_id, callback);
            return;
        }
        session->erase("email_code");
#endif
    
        try
        {
            //check the old password
            orm::Result result = db->execSqlSync("select password from users where user_id=$1", user_id);
            if (result.size() == 0)
            {
                GetLogger(session_id)->Error("Login is incorrect");
                SendError(k401Unauthorized, "Login is incorrect", session_id, callback);
                return;
            }
    
            auto row = result[0];
            std::string stored_hash = row["password"].as<std::string>();
            if (!VerifyPassword(old_password, stored_hash))
            {
                GetLogger(session_id)->Error("Old password is incorrect");
                SendError(k401Unauthorized, "Old password is incorrect", session_id, callback);
                return;
            }
    
            //generate the hash of the password with salt
            std::string hash = HashPassword(password);

            result = db->execSqlSync("update users set password=$1 where user_id=$2", hash, user_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error updating password");
                SendError(k500InternalServerError, "Error updating password", session_id, callback);
                return;
            }
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), session_id, callback);
            return;
        }
    }

    SendOk(callback);
}

void ServiceController::GetUserSettings(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("GetUserSettings request");

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
    {
        GetLogger(session_id)->Error("GetUserSettings error: Empty user_id");
        SendError(k500InternalServerError, "Empty user_id", session_id, callback);
        return;
    }

    try
    {
        //update the existing json, load it, change and save
        orm::Result result = db->execSqlSync("select login, name, email, settings from users where user_id=$1", user_id);
        if (result.size() == 0)
        {
            GetLogger(session_id)->Error("Database error: Error updating settings");
            SendError(k500InternalServerError, "Error updating settings", session_id, callback);
            return;
        }

        auto row = result[0];
        auto settings = row["settings"].as<std::string>();
        if (settings.empty())
            settings = "{}";
        Json::Value s(Json::objectValue);
        Json::Reader reader;
        if (!reader.parse(settings.c_str(), s))
        {
            SendError(k500InternalServerError, "Json error", session_id, callback);
            return;
        }
        Json::Value v(Json::objectValue);
        v["login"] = row["login"].as<std::string>();
        v["name"] = row["name"].as<std::string>();
        v["email"] = row["email"].as<std::string>();
        v["settings"] = s;
        SendJson(callback, v);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
    }
}

void ServiceController::RecoverPassword(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    GetLogger(session_id)->Debug("RecoverPassword request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("RecoverPassword error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    if (!json->isMember("password") || !(*json)["password"].isString() || !json->isMember("email_code") || !(*json)["email_code"].isString())
    {
        GetLogger(session_id)->Error("RecoverPassword error: Wrong request: empty request");
        SendError(k400BadRequest, "Wrong request: empty request", session_id, callback);
        return;
    }

    std::string password, email_code;
    if (json->isMember("password") && (*json)["password"].isString())
        password = (*json)["password"].asString();
    if (json->isMember("email_code") && (*json)["email_code"].isString())
        email_code = (*json)["email_code"].asString();

    if (password.empty() || email_code.empty())
    {
        GetLogger(session_id)->Error("RecoverPassword error: Wrong request: empty request");
        SendError(k400BadRequest, "Wrong request: empty request", session_id, callback);
        return;
    }

    SessionPtr session = req->session();
#ifndef TEST
    if (session->get<std::string>("email_code") != email_code)
    {
        SendError(k400BadRequest, "Wrong email code", session_id, callback);
        return;
    }
#endif
    session->erase("email_code");

    ClearDbTurnOff t; //skip the clear db circles for a while
    orm::DbClientPtr db = app().getDbClient();
    auto email = session->get<std::string>("email_code_email");

    //generate the hash of the password with salt
    std::string hash = HashPassword(password);

    try
    {
        orm::Result result = db->execSqlSync("update users set password=$1 where email=$2", hash, email);
        if (result.affectedRows() == 0)
        {
            GetLogger(session_id)->Error("Database error: Error updating password");
            SendError(k500InternalServerError, "Error updating password", session_id, callback);
            return;
        }
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), session_id, callback);
        return;
    }

    SendOk(callback);
}

void ServiceController::SolverAction(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("SolverAction error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", session_id, callback);
        return;
    }

    std::string guid;
    if (!json->isMember("solver_guid") || !(*json)["solver_guid"].isString())
    {
        SendError(k400BadRequest, "Guid not found in the request", session_id, callback);
        return;
    }
    guid = (*json)["solver_guid"].asString();

    SessionPtr session = req->session();
    if (session->get<std::string>("solver_id") != guid)
    {
        session->erase("solver_id");
        session->insert("solver_id", guid);
        
        auto login = session->get<std::string>("login");
        if (!login.empty())
            GetCalculatorLogger(guid)->Info("Calculator started: {}, login: {}", guid, login);
        if (!login.empty())
            GetSolverLogger(guid)->Info("Solver started: ip={}, guid={}, login: {}", drogon::plugin::RealIpResolver::GetRealAddr(req).toIp(), guid, login);
        else
            GetSolverLogger(guid)->Info("Solver started: ip={}, guid={}", drogon::plugin::RealIpResolver::GetRealAddr(req).toIp(), guid);
    }
    
    std::string id;
    if (json->isMember("id"))
    {
        const Json::Value& val = (*json)["id"];
        Json::FastWriter writer;
        id = writer.write(val);
        if (!id.empty() && id.back() == '\n')
            id.pop_back();
    }

    std::string expression;
    if (json->isMember("expression") && (*json)["expression"].isString())
        expression = (*json)["expression"].asString();

    if (json->isMember("command") && (*json)["command"].isString() && (*json)["command"].asString() == "SOLVE_CODE")
    {
        int expression_type = -1;
        if (json->isMember("expression_type") && (*json)["expression_type"].isInt())
            expression_type = (*json)["expression_type"].asInt();
        std::string results_order;
        if (json->isMember("results_order"))
        {
            Json::FastWriter fast_writer;
            results_order = fast_writer.write((*json)["results_order"]);
            if (!results_order.empty() && results_order[results_order.size() - 1] == '\n')
                results_order.pop_back();
        }

        int result_type = -1;
        if (json->isMember("result_type") && (*json)["result_type"].isInt())
            result_type = (*json)["result_type"].asInt();

        int real_exponent_size = -1;
        if (json->isMember("real_exponent_size") && (*json)["real_exponent_size"].isInt())
            real_exponent_size = (*json)["real_exponent_size"].asInt();
        int real_precision = -1;
        if (json->isMember("real_precision") && (*json)["real_precision"].isInt())
            real_precision = (*json)["real_precision"].asInt();
        int real_default_angle_measure = -1;
        if (json->isMember("real_default_angle_measure") && (*json)["real_default_angle_measure"].isInt())
            real_default_angle_measure = (*json)["real_default_angle_measure"].asInt();
        int real_result_angle_measure = -1;
        if (json->isMember("real_result_angle_measure") && (*json)["real_result_angle_measure"].isInt())
            real_result_angle_measure = (*json)["real_result_angle_measure"].asInt();
        
        int integer_default_notation = -1;
        if (json->isMember("integer_default_notation") && (*json)["integer_default_notation"].isInt())
            integer_default_notation = (*json)["integer_default_notation"].asInt();
        int integer_result_notation = -1;
        if (json->isMember("integer_result_notation") && (*json)["integer_result_notation"].isInt())
            integer_result_notation = (*json)["result_notation"].asInt();
        
        int fraction_form = -1;
        if (json->isMember("fraction_form") && (*json)["fraction_form"].isInt())
            fraction_form = (*json)["fraction_form"].asInt();

        int complex_form = -1;
        if (json->isMember("complex_form") && (*json)["complex_form"].isInt())
            complex_form = (*json)["complex_form"].asInt();
        int complex_default_angle_measure = -1;
        if (json->isMember("complex_default_angle_measure") && (*json)["complex_default_angle_measure"].isInt())
            complex_default_angle_measure = (*json)["complex_default_angle_measure"].asInt();
        int complex_exponent_size = -1;
        if (json->isMember("complex_exponent_size") && (*json)["complex_exponent_size"].isInt())
            complex_exponent_size = (*json)["complex_exponent_size"].asInt();
        int complex_precision = -1;
        if (json->isMember("complex_precision") && (*json)["complex_precision"].isInt())
            complex_precision = (*json)["complex_precision"].asInt();
        int complex_result_angle_measure = -1;
        if (json->isMember("complex_result_angle_measure") && (*json)["complex_result_angle_measure"].isInt())
            complex_result_angle_measure = (*json)["complex_result_angle_measure"].asInt();
        int complex_max_count = -1;
        if (json->isMember("complex_max_count") && (*json)["complex_max_count"].isInt())
            complex_max_count = (*json)["complex_max_count"].asInt();
        
        GetSolverLogger(guid)->Info("SOLVE_CODE: expression={}, expression_type={}, id={}, results_order={}, result_type={}, real_exponent_size={}, "
            "real_precision={}, real_default_angle_measure={}, real_result_angle_measure={}, integer_default_notation={}, integer_result_notation={}, "
            "fraction_form={}, complex_form={}, complex_default_angle_measure={}, complex_exponent_size={}, complex_precision={}, "
            "complex_result_angle_measure={}, complex_max_count={}", expression, expression_type, id, results_order, result_type, real_exponent_size,
            real_precision, real_default_angle_measure, real_result_angle_measure, integer_default_notation, integer_result_notation, 
            fraction_form, complex_form, complex_default_angle_measure, complex_exponent_size, complex_precision, complex_result_angle_measure, complex_max_count);
    }
    else
    {
        Json::FastWriter fastWriter;
        std::string output = fastWriter.write(*json);
        if (!output.empty() && output[output.size() - 1] == '\n')
            output.pop_back();
        GetSolverLogger(guid)->Info("Solve result: {}", output);

        //short calculator log
        std::string mantissa, exponent, numerator, denomerator, integer, value, unit, error;
        if (json->isMember("mantissa") && (*json)["mantissa"].isString())
            mantissa = (*json)["mantissa"].asString();
        if (json->isMember("exponent") && (*json)["exponent"].isString())
            exponent = (*json)["exponent"].asString();
        if (json->isMember("denomerator") && (*json)["denomerator"].isString())
            denomerator = (*json)["denomerator"].asString();
        if (json->isMember("numerator") && (*json)["numerator"].isString())
            numerator = (*json)["numerator"].asString();
        if (json->isMember("integer") && (*json)["integer"].isString())
            integer = (*json)["integer"].asString();
        if (json->isMember("value") && (*json)["value"].isString())
            value = (*json)["value"].asString();
        if (json->isMember("unit") && (*json)["unit"].isObject())
        {
            Json::Value& u = (*json)["unit"];
            if (u.isMember("value") && u["value"].isArray())
            {
                Json::Value arr = u["value"];
                unit = "[";
                for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
                {
                    const Json::Value& v = arr[i];
                    if (v.isMember("name") && v["name"].isString())
                        unit += v["name"].asString();
                    if (v.isMember("power") && v["power"].isInt())
                    {
                        unit += "^";
                        unit += v["power"].asString();
                    }
                    if (i < arr.size() - 1)
                        unit += ",";
                }                   
                unit += "]";
            }
            if (u.isMember("system") && u["system"].isString())
                unit += "{" + u["system"].asString() + "}";
        }
        if (json->isMember("error") && (*json)["error"].isObject())
        {
            Json::Value& err = (*json)["error"];
            if (err.isMember("description") && err["description"].isString())
                error += err["description"].asString();
            else if (err.isMember("error_code") && err["error_code"].isInt())
                error += "error_code:" + err["error_code"].asString();
        }
        if (!expression.empty())
        {
            if (!mantissa.empty() && !exponent.empty())
                GetCalculatorLogger(guid)->Info("{}={}{}{}, id={}", expression, mantissa + "*10^" + exponent, unit, error, id);
            else if (!mantissa.empty())
                GetCalculatorLogger(guid)->Info("{}={}{}{}, id={}", expression, mantissa, unit, error, id);
            else if (!integer.empty() && !numerator.empty() && !denomerator.empty())
                GetCalculatorLogger(guid)->Info("{}={}{}{}, id={}", expression, integer + "(" + numerator + ")/(" + denomerator + ")", unit, error, id);
            else if (!numerator.empty() && !denomerator.empty())
                GetCalculatorLogger(guid)->Info("{}={}{}{}, id={}", expression, "(" + numerator + ")/(" + denomerator + ")", unit, error, id);
            else if (!integer.empty())
                GetCalculatorLogger(guid)->Info("{}={}{}{}, id={}", expression, integer, unit, error, id);
            else if (!value.empty())
                GetCalculatorLogger(guid)->Info("{}={}{}{}, id={}", expression, value, unit, error, id);
            else
            {
                if (json->isMember("results") && (*json)["results"].isArray())
                {
                    std::string res = "[";
                    Json::Value arr = (*json)["results"];
                    for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
                    {
                        const Json::Value& num = arr[i];
                        if (num.isMember("re") && num["re"].isObject())
                        {
                            const Json::Value& u = num["re"];
                            if (u.isMember("mantissa") && u["mantissa"].isString())
                                mantissa = u["mantissa"].asString();
                            if (u.isMember("exponent") && u["exponent"].isString())
                                exponent = u["exponent"].asString();
                            res += mantissa;
                            if (!exponent.empty())
                                res += "*10^" + exponent;
                            if (i < arr.size() - 1)
                                res += ",";
                        }
                        if (num.isMember("im") && num["im"].isObject())
                        {
                            const Json::Value& u = num["im"];
                            if (u.isMember("mantissa") && u["mantissa"].isString())
                                mantissa = u["mantissa"].asString();
                            if (u.isMember("exponent") && u["exponent"].isString())
                                exponent = u["exponent"].asString();
                            res += "+i*" + mantissa;
                            if (!exponent.empty())
                                res += "*10^" + exponent;
                            if (i < arr.size() - 1)
                                res += ",";
                        }
                    }                   
                    res += "]";
                    GetCalculatorLogger(guid)->Info("{}={}{}{}, id={}", expression, res, unit, error, id);
                }
                else
                    GetCalculatorLogger(guid)->Info("{}={}, id={}", expression, error, id);
            }
        }
        else
        {
            GetCalculatorLogger(guid)->Info("{}=", expression);
        }
    }

    SendOk(callback);
}

void ServiceController::SendLibraryDocument(const HttpRequestPtr& req, const std::string& document, std::function<void (const HttpResponsePtr &)>& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    fs::path path;
    try
    {
        //check if the path is inside library_path
        path = fs::canonical(fs::path(document));
        if (!std::string(path.c_str()).starts_with(library_path))
        {
            GetLogger(session_id)->Error("LoadLibraryDocument error: Path not found: {}", path.c_str());
            SendError(k404NotFound, "Path not found", session_id, callback);
            return;
        }

        std::ifstream file(path.string(), std::ios::binary);
        if (!file.is_open())
        {
            GetLogger(session_id)->Error("LoadLibraryDocument error: Path not found: {}", path.c_str());
            SendError(k500InternalServerError, "Error loading file", session_id, callback);
            return;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        try
        {
            //try to open as compressed file
            boost::iostreams::array_source src(content.data(), content.size());
            boost::iostreams::filtering_streambuf<boost::iostreams::input> in;
            in.push(boost::iostreams::gzip_decompressor());
            in.push(src);
            std::stringstream json;
            boost::iostreams::copy(in, json);
            SendJson(callback, json.str(), session_id);
            return;
        }
        catch (const std::ios_base::failure&)
        {
        }

        SendJson(callback, content, session_id);
    }
    catch (const std::exception& ex)
    {
        GetLogger(session_id)->Error("LoadLibraryDocument error: file not open");
        SendError(k404NotFound, "File not open", session_id, callback);
    }
}

void ServiceController::SendFeedback(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the SendFeedback request", session_id, callback);
        return;
    }

    std::string name, email, topic, message, version, platform;
    if (json->isMember("name") && (*json)["name"].isString())
        name = (*json)["name"].asString();
    if (json->isMember("email") && (*json)["email"].isString())
        email = (*json)["email"].asString();
    if (json->isMember("topic") && (*json)["topic"].isString())
        topic = (*json)["topic"].asString();
    if (json->isMember("message") && (*json)["message"].isString())
        message = (*json)["message"].asString();
    if (json->isMember("version") && (*json)["version"].isString())
        version = (*json)["version"].asString();
    if (json->isMember("platform") && (*json)["platform"].isString())
        platform = (*json)["platform"].asString();

    GetLogger(session_id)->Info("SendFeedback request name: {}, email: {}, topic: {}, message: {}", name, email, topic, message);

    if (email.empty() || topic.empty() || message.empty())
    {
        SendError(k400BadRequest, "Required fields are empty", session_id, callback);
        return;
    }

    std::vector<EmailAttachment> attachments;
    if (json->isMember("attachments") && (*json)["attachments"].isArray())
    {
        const Json::Value& arr = (*json)["attachments"];
        for (const auto& item : arr)
        {
            if (!item.isObject())
                continue;
            EmailAttachment at;
            if (item.isMember("filename") && item["filename"].isString())
                at.filename = item["filename"].asString();
            if (item.isMember("content_type") && item["content_type"].isString())
                at.content_type = item["content_type"].asString();
            if (item.isMember("data") && item["data"].isString())
                at.data = item["data"].asString();
            if (!at.filename.empty() && !at.data.empty())
                attachments.push_back(std::move(at));
        }
    }

    std::string subject = "[Feedback] " + topic;
    std::string body = "<html><body>"
        "<p><b>Name:</b> " + HtmlEscape(name) + "</p>"
        "<p><b>Email:</b> " + HtmlEscape(email) + "</p>"
        "<p><b>Topic:</b> " + HtmlEscape(topic) + "</p>"
        "<p><b>Version:</b> " + HtmlEscape(version) + "</p>"
        "<p><b>Platform:</b> " + HtmlEscape(platform) + "</p>"
        "<p>" + HtmlEscape(message) + "</p>"
        "</body></html>";

    const Json::Value& cfg = app().getCustomConfig();
    std::string from = cfg.get("email_name", "no-reply@yutovo.ru").asString();
    std::string to = cfg.get("feedback_email", "support@yutovo.ru").asString();

    CURLcode r = SendEmail(from, to, subject, body, attachments);
    if (r != CURLE_OK)
    {
        SendError(k500InternalServerError, "Failed to send feedback", session_id, callback);
        return;
    }

    SendOk(callback);
}

int ServiceController::CompareVersions(const std::string& a, const std::string& b)
{
    size_t i = 0;
    size_t j = 0;
    while (i < a.size() || j < b.size())
    {
        int num_a = 0;
        int num_b = 0;
        while (i < a.size() && std::isdigit(static_cast<unsigned char>(a[i])))
            num_a = num_a * 10 + (a[i++] - '0');
        while (j < b.size() && std::isdigit(static_cast<unsigned char>(b[j])))
            num_b = num_b * 10 + (b[j++] - '0');
        if (num_a != num_b)
            return num_a - num_b;
        if (i < a.size() && a[i] == '.')
            ++i;
        if (j < b.size() && b[j] == '.')
            ++j;
    }
    return 0;
}

std::string ServiceController::MakeAbsoluteUrl(const HttpRequestPtr& req, const std::string& url)
{
    if (url.empty() || url[0] != '/')
        return url;

    std::string proto = "http";
    if (req->getHeader("X-Forwarded-Proto") == "https")
        proto = "https";
    else if (req->getLocalAddr().toPort() == 443)
        proto = "https";
    return proto + "://" + req->getHeader("Host") + url;
}

void ServiceController::GetUpdates(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr&)>&& callback)
{
    std::string session_id;
    if (!GetSessionId(req, callback, session_id))
        return;

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the GetUpdates request", session_id, callback);
        return;
    }

    std::string language, version, system;
    if (json->isMember("version") && (*json)["version"].isString())
        version = (*json)["version"].asString();
    if (json->isMember("system") && (*json)["system"].isString())
        system = (*json)["system"].asString();
    if (json->isMember("language") && (*json)["language"].isString())
        language = (*json)["language"].asString();

    std::string ip = drogon::plugin::RealIpResolver::GetRealAddr(req).toIp();
    GetUpdatesLogger(session_id)->Info("GetUpdates request version: {}, system: {}, language: {}, ip: {}", version, system, language, ip);

    if (version.empty() || system.empty() || language.empty())
    {
        SendError(k400BadRequest, "Required fields are empty", session_id, callback);
        return;
    }

    std::string document_root = HttpAppFramework::instance().getDocumentRoot();
    std::string downloads_path = document_root + "/downloads/downloads.json";
    std::ifstream downloads_file(downloads_path);
    if (!downloads_file.is_open())
    {
        SendError(k500InternalServerError, "downloads.json not found", session_id, callback);
        return;
    }

    Json::Value downloads;
    Json::Reader reader;
    if (!reader.parse(downloads_file, downloads))
    {
        SendError(k500InternalServerError, "downloads.json parse error", session_id, callback);
        return;
    }

    if (!downloads.isArray())
    {
        SendError(k500InternalServerError, "downloads.json format error", session_id, callback);
        return;
    }

    std::string new_version;
    std::string new_url;
    for (const auto& item : downloads)
    {
        if (!item.isObject())
            continue;
        std::string item_system = item.get("system", "").asString();
        if (item_system != system)
            continue;
        std::string item_version = item.get("version", "").asString();
        std::string item_link = item.get("link", "").asString();
        if (item_version.empty() || CompareVersions(version, item_version) >= 0)
            continue;
        new_version = item_version;
        new_url = item_link;
        break;
    }

    Json::Value response;
    if (!new_version.empty())
    {
        response["hasUpdate"] = true;
        response["version"] = new_version;
        response["url"] = MakeAbsoluteUrl(req, new_url);
    }
    else
    {
        response["hasUpdate"] = false;
    }
    SendJson(callback, response);
}

};
