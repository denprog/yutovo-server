#include "service_controller.h"
#include "../logic/clear_db.h"
#include <functional>

namespace yutovo_server
{

namespace fs = std::filesystem;
using namespace std::chrono_literals;

//ServiceController

ServiceController::ServiceController()
{
    const Json::Value& v = app().getCustomConfig();
    tasks_path = v.get("tasks_path", "").asString();
    if (tasks_path.empty())
        throw std::system_error(ENOTDIR, std::generic_category(), "Tasks path not defined");
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
                logger->Error("ServiceController not connected to Solver");
                return;
            }
            logger->Info("ServiceController connected to Solver");
            solver_client->getConnection()->setPingMessage("", 2s);
        });
}

void ServiceController::GetTasks(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    logger->Info("GetTasks request");

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
        SendError(k400BadRequest, "Wrong request", callback);
        return;
    }
    task = (*json)["task"].asString();
    
    logger->Info("LoadTask request task={}", "/" + language + task);
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
    }
    catch (const std::exception& ex)
    {
        SendError(k404NotFound, "Path not found", callback);
        return;
    }
    
    SendFile(callback, path);
}

void ServiceController::ListIdentifiers(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    if (!solver_client)
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
    logger->Info("NewDocument request");

    auto json = req->getJsonObject();
    if (!json)
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    std::string session_id = req->getCookie("session_id");
    if (session_id.empty())
    {
        SendError(k400BadRequest, "Wrong request", callback);
        return;
    }

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
        user_id = "-1";
    std::string document_id = session->get<std::string>("document_id");

    ClearDbTurnOff t; //skip the clear db circles for a while

    try
    {
        if (user_id == "-1")
        {
            //for unregistered user reset his document to empty
            db->execSqlSync("update user_documents set document='{}' where document_id=$1", document_id);
        }
        else
        {
            //for registered user create a new document
            if (!AddDocument(user_id, document_id))
            {
                logger->Error("Database error: Error inserting a document");
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
                    logger->Error("Database error: Error inserting a document");
                    SendError(k500InternalServerError, "Error inserting a document", callback);
                    return;
                }
            }

            db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);
        }
  
        SendOk(callback, document_id);
    }
    catch (const orm::DrogonDbException& e)
    {
        logger->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::SaveDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    logger->Info("SaveDocument request");
    auto json = req->getJsonObject();
    if (!json)
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
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
        SendError(k400BadRequest, "Wrong request", callback);
        return;
    }

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    std::string session_id = session->get<std::string>("session_id");
    if (user_id.empty())
        user_id = "-1";
    std::string document_id = "-1";

    ClearDbTurnOff t; //skip the clear db circles

    try
    {
        orm::Result result = db->execSqlSync("select document_id from user_sessions where session_id=$1", session_id);
        if (result.size() == 0)
        {
            logger->Error("Database error: document not found");
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
                logger->Error("Database error: document not found");
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
        logger->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
        return;
    }

    auto id = text["id"].asString();
    if (id == "0")
    {
        //update whole document
        try
        {
            if (document_id == "-1")
            {
                //insert new document
                if (!AddDocument(user_id, document_id))
                {
                    logger->Error("Database error: Error inserting a document");
                    SendError(k500InternalServerError, "Error inserting a document", callback);
                    return;
                }
            }

            orm::Result result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", doc.toStyledString(), document_id);
            if (result.affectedRows() == 0)
            {
                if (!AddDocument(user_id, document_id))
                {
                    logger->Error("Database error: Error inserting a document");
                    SendError(k500InternalServerError, "Error inserting a document", callback);
                    return;
                }
            }

            db->execSqlSync("update user_sessions set document_id=$1 where session_id=$2", document_id, session_id);
            SendOk(callback, document_id);
        }
        catch (const orm::DrogonDbException& e)
        {
            logger->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
        }
        return;
    }

    //update a part of the document
    try
    {
        if (document_id == "-1")
        {
            logger->Error("Wrong document id for session_id={}", session_id);
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
        logger->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::LoadDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    logger->Info("LoadDocument request");

    auto json = req->getJsonObject();
    if (!json)
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    std::string document_id = req->getCookie("document_id");
    if (document_id.empty())
    {
        SendError(k400BadRequest, "Wrong request", callback);
        return;
    }

    std::string id;
    if (json->isObject() && json->isMember("id") && (*json)["id"].isString())
        id = (*json)["id"].asString();

    ClearDbTurnOff t; //skip the clear db circles for a while

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();

    try
    {
        //check this user can load this document
        orm::Result result = db->execSqlSync("select user_id, public from user_documents where document_id=$1", document_id);
        if (result.size() == 0)
        {
            SendError(k400BadRequest, "No such document", callback);
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
    }
    catch (const orm::DrogonDbException& e)
    {
        logger->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

};
