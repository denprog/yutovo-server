#include "service_controller.h"
#include "../logic/clear_db.h"
#include <functional>
#include <fstream>

namespace yutovo_server
{

namespace fs = std::filesystem;
using namespace std::chrono_literals;

//ServiceController

ServiceController::ServiceController()
{
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
}

void ServiceController::GetTasks(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("GetTasks request");

    auto json = req->getJsonObject();
    std::string language = "en";
    if (json && json->isObject() && json->isMember("language") && (*json)["language"].isString())
        language = (*json)["language"].asString();

    std::function<void (const fs::path& path, Json::Value& json)> get_files = 
        [&](const fs::path& path, Json::Value& json)
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                if (entry.is_directory())
                {
                    Json::Value e;
                    auto s = entry.path().string();
                    json[entry.path().stem().string()] = e;
                    auto& d = json[entry.path().stem().string()];
                    get_files(entry.path(), d);
                }
                else if (entry.is_regular_file())
                {
                    if (json.isMember("files"))
                    {
                        Json::Value& files = json["files"];
                        files.append(entry.path().stem().c_str());
                    }
                    else
                    {
                        Json::Value files(Json::arrayValue);
                        files.append(entry.path().stem().c_str());
                        json["files"] = files;
                    }
                }
            }
        };
    
    Json::Value root;
    get_files(fs::path(tasks_path + "/" + language), root);

    SendJson(callback, root);
}

void ServiceController::LoadTask(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    std::string language = "en";
    if (json->isMember("language") && (*json)["language"].isString())
        language = (*json)["language"].asString();

    std::string task;
    if (!json->isMember("task") || !(*json)["task"].isString())
    {
        SendError(k400BadRequest, "Wrong request: empty task", callback);
        return;
    }
    task = (*json)["task"].asString();
    
    GetLogger(session_id)->Info("LoadTask request task={}", "/" + language + task);
    task = tasks_path + language + task + ".yut";
    fs::path path;

    //check if the path is inside tasks_path
    try
    {
        path = fs::canonical(fs::path(task));
        if (!std::string(path.c_str()).starts_with(tasks_path))
        {
            SendError(k404NotFound, "Path not found", callback);
            return;
        }

        SendFile(callback, path);
    }
    catch (const std::exception& ex)
    {
        SendError(k404NotFound, "Path not found", callback);
    }
}

void ServiceController::SaveTask(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    std::string language = "en";
    if (json->isMember("language") && (*json)["language"].isString())
        language = (*json)["language"].asString();

    std::string task;
    if (!json->isMember("task") || !(*json)["task"].isString())
    {
        SendError(k400BadRequest, "Wrong request: empty task", callback);
        return;
    }
    task = (*json)["task"].asString();
    size_t p = task.find_last_of("/");
    if (p == std::string::npos || p >= task.length())
    {
        SendError(k400BadRequest, "Wrong task name", callback);
        return;
    }
    
    std::string name = task.substr(p + 1);
    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
        user_id = "-1";
    if (session_id.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty session_id", callback);
        return;
    }

    std::string document_id;

    GetLogger(session_id)->Info("task={}", task);

    ClearDbTurnOff t; //skip the clear db circles

    try
    {
        if (!AddDocument(user_id, document_id, name))
        {
            GetLogger(session_id)->Error("Database error: Error inserting a document");
            SendError(k500InternalServerError, "Error inserting a document", callback);
            return;
        }

        task = tasks_path + language + task + ".yut";
        fs::path path;
        Json::Value doc;

        //check if the path is inside tasks_path
        try
        {
            path = fs::canonical(fs::path(task));
            if (!std::string(path.c_str()).starts_with(tasks_path))
            {
                SendError(k404NotFound, "Path not found", callback);
                return;
            }

            std::ifstream f(path.string().c_str());
            f >> doc;
        }
        catch (const std::exception& ex)
        {
            SendError(k404NotFound, "Path not found", callback);
            return;
        }

        orm::Result result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", doc.toStyledString(), document_id);
        if (result.affectedRows() == 0)
        {
            GetLogger(session_id)->Error("Database error: Error inserting a document");
            SendError(k500InternalServerError, "Error inserting a document", callback);
            return;
        }

        db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);
        SendOk(callback, document_id, name);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::ListIdentifiers(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    if (!solver_client || !solver_client->getConnection())
    {
        SendError(k500InternalServerError, "Solver socket not open", callback);
        return;
    }
    
    auto json = req->getJsonObject();
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
            SendError(k500InternalServerError, "Solver not response", callback);
            return;
        }
        std::this_thread::sleep_for(1ms);
    }
    
    SendJson(callback, solver_response);
}

void ServiceController::NewDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("NewDocument request");

    auto json = req->getJsonObject();
    if (!json)
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }
    if (session_id.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty session_id", callback);
        return;
    }

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
        user_id = "-1";
    std::string document_id = session->get<std::string>("document_id");
    GetLogger(session_id)->Info("document_id={}", document_id);

    ClearDbTurnOff t; //skip the clear db circles for a while

    try
    {
        if (user_id == "-1")
        {
            //for unregistered user reset his document to empty
            db->execSqlSync("update user_documents set document='{}' where document_id=$1", document_id);
            SendOk(callback, document_id);
        }
        else
        {
            std::string name;
            if (json->isObject() && json->isMember("name") && (*json)["name"].isString())
                name = (*json)["name"].asString();
            
            //for registered user create a new document
            if (!AddDocument(user_id, document_id, name))
            {
                GetLogger(session_id)->Error("Database error: Error inserting a document");
                SendError(k500InternalServerError, "Error inserting a document", callback);
                return;
            }

            Json::Value text;
            Json::Value& doc = *json;
            if (doc.isObject() && doc.isMember("text"))
            {
                orm::Result result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", doc.toStyledString(), document_id);
                if (result.affectedRows() == 0)
                {
                    GetLogger(session_id)->Error("Database error: Error inserting a document");
                    SendError(k500InternalServerError, "Error inserting a document", callback);
                    return;
                }
            }

            if (json->isObject() && json->isMember("json"))
            {
                orm::Result result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", 
                    (*json)["json"].isString() ? (*json)["json"].asString() : (*json)["json"].toStyledString(), document_id);
                if (result.affectedRows() == 0)
                {
                    GetLogger(session_id)->Error("Database error: Error inserting a document");
                    SendError(k500InternalServerError, "Error inserting a document", callback);
                    return;
                }
            }

            db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);

            SendOk(callback, document_id, name);
        }
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::SaveDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("SaveDocument request");
    auto json = req->getJsonObject();
    if (!json)
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }
    if (session_id.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty session_id", callback);
        return;
    }

    Json::Value& doc = *json;
    if (!doc.isObject() || !doc.isMember("text"))
    {
        SendError(k400BadRequest, "Text field not found in the request", callback);
        return;
    }

    Json::Value& text = doc["text"];
    orm::DbClientPtr db = app().getDbClient();

    if (!text.isMember("id") || text["id"].asString().empty())
    {
        SendError(k400BadRequest, "Wrong request: empty id", callback);
        return;
    }

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
        user_id = "-1";
    std::string document_id = "-1";

    try
    {
        orm::Result result = db->execSqlSync("select document_id from user_sessions where session_id=$1", session_id);
        if (result.size() == 0)
        {
            GetLogger(session_id)->Error("Database error: document not found");
            SendError(k500InternalServerError, "Document not found", callback);
            return;
        }

        auto row = result[0];
        document_id = row["document_id"].as<std::string>();

        if (document_id != "-1")
        {
            result = db->execSqlSync("select user_id, shared from user_documents where document_id=$1", document_id);
            if (result.size() == 0)
            {
                GetLogger(session_id)->Error("Database error: document not found");
                SendError(k500InternalServerError, "Document not found", callback);
                return;
            }

            auto row = result[0];
            if (row["user_id"].as<std::string>() != user_id && !row["shared"].as<bool>())
            {
                //saving this foreign document is prohibited
                SendError(k403Forbidden, "Document saving is prohibited", callback);
                return;
            }
        }
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
        return;
    }

    auto id = text["id"].asString();
    if (id == "0")
    {
        //update whole document
        try
        {
            std::string name;
            if (document_id == "-1")
            {
                //insert new document
                if (!AddDocument(user_id, document_id, name))
                {
                    GetLogger(session_id)->Error("Database error: Error inserting a document");
                    SendError(k500InternalServerError, "Error inserting a document", callback);
                    return;
                }
            }

            orm::Result result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", doc.toStyledString(), document_id);
            if (result.affectedRows() == 0)
            {
                if (!AddDocument(user_id, document_id, name))
                {
                    GetLogger(session_id)->Error("Database error: Error inserting a document");
                    SendError(k500InternalServerError, "Error inserting a document", callback);
                    return;
                }
            }

            db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);
            SendOk(callback, document_id, name);
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
        }
        return;
    }

    //update a part of the document
    try
    {
        if (document_id == "-1")
        {
            GetLogger(session_id)->Error("Wrong document id for session_id={}", session_id);
            SendError(k400BadRequest, "Wrong document id", callback);
            return;
        }

        //parse the string id
        std::vector<int> _id;
        if (!ParseId(id, _id))
        {
            SendError(k400BadRequest, "Wrong Id", callback);
            return;
        }

        std::string path = "'{\"text\"";
        for (size_t i = 1; i < _id.size(); ++i)
            path += ",\"elements\"," + std::to_string(_id[i]);
        path += "}'";

        orm::Result result = db->execSqlSync("update user_documents set document=jsonb_set(document," + path + 
            ",jsonb '" + text.toStyledString() + "') where document_id=$1", document_id);
        result = db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);
        SendOk(callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::SaveAsDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("SaveAsDocument request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    Json::Value& doc = *json;
    std::string document_id;
    if (json->isMember("document_id") && ((*json)["document_id"].isInt() || (*json)["document_id"].isString()))
        document_id = (*json)["document_id"].asString();
    if (document_id.empty() || document_id == "-1")
    {
        SendError(k400BadRequest, "document_id field not found in the request", callback);
        return;
    }

    std::string name;
    if (json->isMember("name") || (*json)["name"].isString())
        name = (*json)["name"].asString();
    if (name.empty())
    {
        GetLogger(session_id)->Error("Empty name field");
        SendError(k400BadRequest, "name field not found in the request", callback);
        return;
    }

    GetLogger(session_id)->Info("Name: {}", name);

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
    {
        SendError(k400BadRequest, "Wrong user_id", callback);
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
            SendError(k500InternalServerError, "Document not found", callback);
            return;
        }

        auto row = result[0];
        
        result = db->execSqlSync("select 1 from user_documents where user_id=$1 and name=$2", user_id, name); //check the document name is unique
        if (result.size() > 0)
        {
            GetLogger(session_id)->Error("Database error: document with such name already exists");
            SendError(k409Conflict, "Document with such name already exists", callback);
            return;
        }

        auto doc = row["document"].as<std::string>();

        if (!AddDocument(user_id, document_id, name))
        {
            GetLogger(session_id)->Error("Database error: Error inserting a document");
            SendError(k500InternalServerError, "Error inserting a document", callback);
            return;
        }

        result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", doc, document_id);
        if (result.affectedRows() == 0)
        {
            GetLogger(session_id)->Error("Database error: Error inserting a document");
            SendError(k500InternalServerError, "Error inserting a document", callback);
            return;
        }

        SendOk(callback, document_id);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::LoadDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("LoadDocument request");

    auto json = req->getJsonObject();
    if (!json)
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    SessionPtr session = req->session();
    std::string document_id;
    if (json->isObject() && json->isMember("document_id") && ((*json)["document_id"].isInt() || (*json)["document_id"].isString()))
        document_id = (*json)["document_id"].asString();
    if (document_id.empty() || document_id == "-1")
        document_id = req->getCookie("document_id");
    if (document_id.empty() || document_id == "-1")
        session->get<std::string>("document_id");
    if (document_id.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty document_id", callback);
        return;
    }

    GetLogger(session_id)->Info("document_id={}", document_id);

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
            SendError(k404NotFound, "No such document", callback);
            return;
        }

        auto row = result[0];
        if (row["user_id"].as<std::string>() != user_id)
        {
            if (!row["public"].as<bool>())
            {
                SendError(k403Forbidden, "This document is not public", callback);
                return;
            }
        }

        if (id.empty())
        {
            //get the whole document
            result = db->execSqlSync("select document from user_documents where document_id=$1", document_id);
            auto row = result[0];
            SendJson(callback, row["document"].as<std::string>());
        }
        else
        {
            //get a part of the document
            std::vector<int> _id;
            if (!ParseId(id, _id))
            {
                SendError(k400BadRequest, "Wrong Id", callback);
                return;
            }

            std::string path = "'text'";
            for (size_t i = 1; i < _id.size(); ++i)
                path += "->'elements'->" + std::to_string(_id[i]);

            result = db->execSqlSync("select document->" + path + " as document from user_documents where document_id=$1", document_id);
            if (result.size() == 0)
            {
                SendError(k400BadRequest, "No such session", callback);
                return;
            }

            auto row = result[0];
            SendJson(callback, row["document"].as<std::string>());
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
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::DeleteDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("DeleteDocument request");

    auto json = req->getJsonObject();
    if (!json)
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    std::string document_id;
    if (json->isObject() && json->isMember("document_id") && (*json)["document_id"].isInt())
        document_id = (*json)["document_id"].asString();
    else
        document_id = req->getCookie("document_id");
    if (document_id.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty document_id", callback);
        return;
    }

    GetLogger(session_id)->Info("document_id={}", document_id);

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (session_id.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty session_id", callback);
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
            SendError(k403Forbidden, "Cannot delete document", callback);
            return;
        }

        db->execSqlSync("delete from user_documents where document_id=$1", document_id);
        db->execSqlSync("update user_sessions set document_id=-1 where session_id=$1", session_id);
        SendOk(callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::ListDocuments(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("ListDocuments request");

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
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::GetDocumentName(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("GetDocumentName request");

    auto json = req->getJsonObject();
    if (!json)
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    std::string document_id;
    if (json->isObject() && json->isMember("document_id") && ((*json)["document_id"].isInt() || (*json)["document_id"].isString()))
        document_id = (*json)["document_id"].asString();
    else
        document_id = req->getCookie("document_id"); //this request is for current document
    if (document_id.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty document_id", callback);
        return;
    }

    GetLogger(session_id)->Info("document_id={}", document_id);

    ClearDbTurnOff t; //skip the clear db circles for a while

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();

    try
    {
        orm::Result result = db->execSqlSync("select user_id, name, public from user_documents where document_id=$1", document_id);
        if (result.size() == 0)
        {
            SendError(k404NotFound, "No such document", callback);
            return;
        }

        auto row = result[0];
        if (row["user_id"].as<std::string>() != session->get<std::string>("user_id"))
        {
            if (!row["public"].as<bool>())
            {
                SendError(k403Forbidden, "This document is not public", callback);
                return;
            }
        }

        Json::Value v(Json::objectValue);
        v["name"] = row["name"].as<std::string>();
        SendJson(callback, v);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::RenameDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("RenameDocument request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    std::string document_id;
    if (json->isMember("document_id") && ((*json)["document_id"].isInt()) || (*json)["document_id"].isString())
        document_id = (*json)["document_id"].asString();
    else
        document_id = req->getCookie("document_id"); //this request is for current document
    if (document_id.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty document_id", callback);
        return;
    }

    std::string name;
    if (json->isMember("name") && (*json)["name"].isString())
        name = (*json)["name"].asString();
    if (name.empty())
    {
        SendError(k400BadRequest, "Wrong request: empty name", callback);
        return;
    }

    GetLogger(session_id)->Info("document_id={}", document_id);

    ClearDbTurnOff t; //skip the clear db circles for a while

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();

    try
    {
        orm::Result result = db->execSqlSync("update user_documents set name=$1 where document_id=$2", name, document_id);
        if (result.affectedRows() == 0)
        {
            GetLogger(session_id)->Error("Database error: Error updaing a document");
            SendError(k500InternalServerError, "Error updating a document", callback);
            return;
        }

        SendOk(callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

};
