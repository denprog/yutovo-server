#include "service_controller.h"
#include "../logic/clear_db.h"
#include <functional>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

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
    session_id = req->getCookie("session_id");
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
                files.append(entry.stem().c_str());
            }
            json["files"] = files;
        };
    
    Json::Value root;
    get_files(fs::path(library_path + "/" + language), "library", root);

    SendJson(callback, root);
}

void ServiceController::LoadLibraryDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("LoadLibraryDocument error: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    std::string language = "en";
    if (json->isMember("language") && (*json)["language"].isString())
        language = (*json)["language"].asString();

    std::string document;
    if (!json->isMember("document") || !(*json)["document"].isString())
    {
        GetLogger(session_id)->Error("LoadLibraryDocument error: Wrong request: empty document");
        SendError(k400BadRequest, "Wrong request: empty document", callback);
        return;
    }
    document = (*json)["document"].asString();
    
    GetLogger(session_id)->Info("LoadLibraryDocument request document={}", "/" + language + document);
    document = library_path + language + document;
    if (!document.ends_with(".yut"))
        document += fs::path(".yut");
    fs::path path;

    //check if the path is inside library_path
    try
    {
        path = fs::canonical(fs::path(document));
        if (!std::string(path.c_str()).starts_with(library_path))
        {
            GetLogger(session_id)->Error("LoadLibraryDocument error: Path not found: {}", path.c_str());
            SendError(k404NotFound, "Path not found", callback);
            return;
        }

        SendFile(callback, path);
    }
    catch (const std::exception& ex)
    {
        GetLogger(session_id)->Error("LoadLibraryDocument error: Path not found");
        SendError(k404NotFound, "Path not found", callback);
    }
}

void ServiceController::SaveLibraryDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("SaveLibraryDocument error: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    std::string language = "en";
    if (json->isMember("language") && (*json)["language"].isString())
        language = (*json)["language"].asString();

    std::string document;
    if (!json->isMember("document") || !(*json)["document"].isString())
    {
        GetLogger(session_id)->Error("SaveLibraryDocument error: Wrong request: empty document");
        SendError(k400BadRequest, "Wrong request: empty document", callback);
        return;
    }
    document = (*json)["document"].asString();
    size_t p = document.find_last_of("/");
    if (p == std::string::npos || p >= document.length())
    {
        GetLogger(session_id)->Error("SaveLibraryDocument error: Wrong document name");
        SendError(k400BadRequest, "Wrong document name", callback);
        return;
    }
    
    std::string name = document.substr(p + 1);
    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
        user_id = "-1";
    if (session_id.empty())
    {
        GetLogger(session_id)->Error("SaveLibraryDocument error: Wrong request: empty session_id");
        SendError(k400BadRequest, "Wrong request: empty session_id", callback);
        return;
    }

    std::string document_id;

    GetLogger(session_id)->Debug("SaveLibraryDocument request: document={}", document);

    ClearDbTurnOff t; //skip the clear db circles

    try
    {
        if (!AddDocument(user_id, document_id, name))
        {
            GetLogger(session_id)->Error("Database error: Error inserting a document: {}, {}", document_id, name);
            SendError(k500InternalServerError, "Error inserting a document", callback);
            return;
        }

        document = library_path + language + document + ".yut";
        fs::path path;
        Json::Value doc;

        //check if the path is inside library_path
        try
        {
            path = fs::canonical(fs::path(document));
            if (!std::string(path.c_str()).starts_with(library_path))
            {
                GetLogger(session_id)->Error("SaveLibraryDocument error: Path not found: {}", path.c_str());
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

#ifdef REMOTE_SOLVER
void ServiceController::ListIdentifiers(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    if (!solver_client || !solver_client->getConnection())
    {
        GetLogger(session_id)->Error("ListIdentifiers error: Solver socket not open");
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
            GetLogger(session_id)->Error("ListIdentifiers error: Solver not response");
            SendError(k500InternalServerError, "Solver not response", callback);
            return;
        }
        std::this_thread::sleep_for(1ms);
    }
    
    SendJson(callback, solver_response);
}
#endif

void ServiceController::NewDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("NewDocument request");

    auto json = req->getJsonObject();
    if (!json)
    {
        GetLogger(session_id)->Error("NewDocument error: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }
    if (session_id.empty())
    {
        GetLogger(session_id)->Error("NewDocument error: Wrong request: empty session_id");
        SendError(k400BadRequest, "Wrong request: empty session_id", callback);
        return;
    }

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
            SendOk(callback, document_id);
        }
        else
        {
            //check count of files
            int max_files = session->get<int>("max_files");
            orm::Result result = db->execSqlSync("select count(*) from user_documents where user_id=$1", user_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error getting count of documents");
                SendError(k500InternalServerError, "Error inserting a document", callback);
                return;
            }

            if (result[0]["count"].as<int>() >= max_files)
            {
                GetLogger(session_id)->Error("Max files count exceed");
                SendError(k403Forbidden, "Max files count exceed", callback);
                return;
            }

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
                result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", doc.toStyledString(), document_id);
                if (result.affectedRows() == 0)
                {
                    GetLogger(session_id)->Error("Database error: Error inserting a document: document_id={}", document_id);
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
                    GetLogger(session_id)->Error("Database error: Error inserting a document: document_id={}", document_id);
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
    GetLogger(session_id)->Debug("SaveDocument request");
    auto json = req->getJsonObject();
    if (!json)
    {
        GetLogger(session_id)->Error("SaveDocument error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }
    if (session_id.empty())
    {
        GetLogger(session_id)->Error("SaveDocument error: Wrong request: empty session_id");
        SendError(k400BadRequest, "Wrong request: empty session_id", callback);
        return;
    }

    Json::Value& doc = *json;
    if (!doc.isObject() || !doc.isMember("text"))
    {
        GetLogger(session_id)->Error("SaveDocument error: Text field not found in the request");
        SendError(k400BadRequest, "Text field not found in the request", callback);
        return;
    }

    Json::Value& text = doc["text"];
    orm::DbClientPtr db = app().getDbClient();

    if (!text.isMember("id") || text["id"].asString().empty())
    {
        GetLogger(session_id)->Error("SaveDocument error: Wrong request: empty id");
        SendError(k400BadRequest, "Wrong request: empty id", callback);
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
            SendError(k500InternalServerError, "Document not found", callback);
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
                SendError(k500InternalServerError, "Document not found", callback);
                return;
            }

            auto row = result[0];
            if (row["user_id"].as<std::string>() != user_id && !row["shared"].as<bool>())
            {
                //saving this foreign document is prohibited
                GetLogger(session_id)->Error("SaveDocument error: Document saving is prohibited: document_id={}", document_id);
                SendError(k403Forbidden, "Document saving is prohibited", callback);
                return;
            }
            document_size = row["pg_column_size"].as<int>();
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
            auto document = doc.toStyledString();
            if (document.size() > max_file_size)
            {
                GetLogger(session_id)->Error("Document size is more then limit");
                SendError(k400BadRequest, "Document size is more then limit", callback);
                return;
            }

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

            orm::Result result = db->execSqlSync("update user_documents set document=$1 where document_id=$2", document, document_id);
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

        auto document = text.toStyledString();
        auto s = document.size();

        //firstly check the size
        if (document_size + document.size() > max_file_size)
        {
            GetLogger(session_id)->Error("Document size is more then limit");
            SendError(k400BadRequest, "Document size is more then limit", callback);
            return;
        }

        orm::Result result = db->execSqlSync("update user_documents set document=jsonb_set(document," + path + 
            ",jsonb '" + document + "') where document_id=$1", document_id);
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
    GetLogger(session_id)->Debug("SaveAsDocument request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("SaveAsDocument error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    Json::Value& doc = *json;
    std::string document_id;
    if (json->isMember("document_id") && ((*json)["document_id"].isInt() || (*json)["document_id"].isString()))
        document_id = (*json)["document_id"].asString();
    if (document_id.empty() || document_id == "-1")
    {
        GetLogger(session_id)->Error("SaveAsDocument error: Wrong request: document_id field not found in the request");
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

    GetLogger(session_id)->Info("Save document: name: {}", name);

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
    {
        GetLogger(session_id)->Error("SaveAsDocument error: Wrong user_id");
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
    GetLogger(session_id)->Debug("LoadDocument request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("LoadDocument error: Wrong request: Json not found in the request");
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
        GetLogger(session_id)->Error("LoadDocument error: Wrong request: empty document_id");
        SendError(k400BadRequest, "Wrong request: empty document_id", callback);
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
            SendError(k404NotFound, "No such document", callback);
            return;
        }

        auto row = result[0];
        if (row["user_id"].as<std::string>() != user_id)
        {
            if (!row["public"].as<bool>())
            {
                GetLogger(session_id)->Error("LoadDocument error: This document is not public: document_id={}", document_id);
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
                GetLogger(session_id)->Error("LoadDocument error: Wrong Id: document_id={}", document_id);
                SendError(k400BadRequest, "Wrong Id", callback);
                return;
            }

            std::string path = "'text'";
            for (size_t i = 1; i < _id.size(); ++i)
                path += "->'elements'->" + std::to_string(_id[i]);

            result = db->execSqlSync("select document->" + path + " as document from user_documents where document_id=$1", document_id);
            if (result.size() == 0)
            {
                GetLogger(session_id)->Error("LoadDocument error: No such session: document_id={}", document_id);
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
    GetLogger(session_id)->Debug("DeleteDocument request");

    auto json = req->getJsonObject();
    if (!json)
    {
        GetLogger(session_id)->Error("DeleteDocument error: Wrong request: Json not found in the request");
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
        GetLogger(session_id)->Error("DeleteDocument error: Wrong request: empty document_id");
        SendError(k400BadRequest, "Wrong request: empty document_id", callback);
        return;
    }

    GetLogger(session_id)->Info("Delete document: document_id={}", document_id);

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
            GetLogger(session_id)->Error("DeleteDocument error: Cannot delete document: document_id={}", document_id);
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
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::GetDocumentId(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Debug("GetDocumentId request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("GetDocumentId error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    ClearDbTurnOff t; //skip the clear db circles for a while
    orm::DbClientPtr db = app().getDbClient();

    std::string document_id;
    if (!json->isMember("name") || !(*json)["name"].isString())
    {
        GetLogger(session_id)->Error("GetDocumentId error: Wrong request: name not found in the request");
        SendError(k400BadRequest, "Name not found in the request", callback);
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
                SendError(k404NotFound, "No such document", callback);
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
            SendError(k500InternalServerError, e.base().what(), callback);
        }
    }
}

void ServiceController::GetDocumentName(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Debug("GetDocumentName request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("GetDocumentName error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", callback);
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
        SendError(k400BadRequest, "Wrong request: empty document_id or name", callback);
        return;
    }

    if (!name.empty())
    {
        std::string lang = "en";
        if (json->isMember("lang") && (*json)["lang"].isString())
            lang = (*json)["lang"].asString();
        std::string document = library_path + lang + name;
        if (!document.ends_with(".yut"))
            document += fs::path(".yut");

        GetLogger(session_id)->Info("Get document name: name={}, lang={}", name, lang);

        fs::path path;
        try
        {
            path = fs::canonical(fs::path(document));
        }
        catch (const std::exception& ex)
        {
            GetLogger(session_id)->Error("GetDocumentName error: Path not found: {}", document);
            SendError(k404NotFound, "Path not found", callback);
            return;
        }
    
        if (!fs::exists(path))
        {
            GetLogger(session_id)->Error("GetDocumentName error: No such document: {}", name);
            SendError(k404NotFound, "No such document", callback);
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
            SendError(k404NotFound, "No such document", callback);
            return;
        }

        auto row = result[0];
        if (row["user_id"].as<std::string>() != session->get<std::string>("user_id"))
        {
            if (!row["public"].as<bool>())
            {
                GetLogger(session_id)->Error("GetDocumentName error: This document is not public: {}", document_id);
                SendError(k403Forbidden, "This document is not public", callback);
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
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::RenameDocument(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Debug("RenameDocument request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("RenameDocument error: Json not found in the request");
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
        GetLogger(session_id)->Error("RenameDocument error: Wrong request: empty document_id");
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

void ServiceController::SetUserSettings(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Debug("SetUserSettings request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("SetUserSettings error: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
    {
        GetLogger(session_id)->Error("SetUserSettings error: Empty user_id");
        SendError(k500InternalServerError, "Empty user_id", callback);
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
        SendError(k400BadRequest, "Wrong request: empty request", callback);
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
            SendError(k500InternalServerError, "Error updating settings", callback);
            return;
        }

        try
        {
            //update the existing json, load it, change and save
            orm::Result result = db->execSqlSync("select settings from users where user_id=$1", user_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error updating settings");
                SendError(k500InternalServerError, "Error updating settings", callback);
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
                    SendError(k500InternalServerError, "Error updating settings", callback);
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
    
            result = db->execSqlSync("update users set settings=$1 where user_id=$2", s_val.toStyledString(), user_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error updating settings");
                SendError(k500InternalServerError, "Error updating settings", callback);
                return;
            }
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
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
                SendError(k500InternalServerError, "Error updating name", callback);
                return;
            }
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
            return;
        }
    }

    if (!email.empty()) //set e-mail
    {
        try
        {
            //check the e-mail doesn't exist
            orm::Result result = db->execSqlSync("select 1 from users where email=$1", email);
            if (result.affectedRows() != 0)
            {
                GetLogger(session_id)->Error("e-mail already exists: {}", email);
                SendError(k409Conflict, "e-mail already exists", callback);
                return;
            }

            result = db->execSqlSync("update users set email=$1 where user_id=$2", email, user_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error updating email");
                SendError(k500InternalServerError, "Error updating email", callback);
                return;
            }
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
            return;
        }
    }

    if (!password.empty() && !old_password.empty()) //set new password
    {
#ifndef TEST
        auto captcha = (*json)["captcha"].asString();
        if (captcha.empty() || session->get<std::string>("captcha") != captcha)
        {
            SendError(k400BadRequest, "Wrong captcha", callback);
            return;
        }
        auto email_code = (*json)["email_code"].asString();
        if (email_code.empty() || session->get<std::string>("email_code") != email_code)
        {
            SendError(k400BadRequest, "Wrong email code", callback);
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
                SendError(k401Unauthorized, "Login is incorrect", callback);
                return;
            }
    
            auto row = result[0];
            std::string hash = row["password"].as<std::string>();
            std::string salt = hash.substr(0, 32);
            std::string h = GetHash(old_password, salt);
            if (salt + h != hash)
            {
                GetLogger(session_id)->Error("Old password is incorrect");
                SendError(k401Unauthorized, "Old password is incorrect", callback);
                return;
            }
    
            //generate the hash of the password with salt
            salt = std::string(boost::lexical_cast<std::string>(boost::uuids::random_generator()()));
            salt.erase(std::remove(salt.begin(), salt.end(), '-'), salt.end());
            hash = GetHash(password, salt);

            result = db->execSqlSync("update users set password=$1 where user_id=$2", salt + hash, user_id);
            if (result.affectedRows() == 0)
            {
                GetLogger(session_id)->Error("Database error: Error updating password");
                SendError(k500InternalServerError, "Error updating password", callback);
                return;
            }
        }
        catch (const orm::DrogonDbException& e)
        {
            GetLogger(session_id)->Error("Database error: {}", e.base().what());
            SendError(k500InternalServerError, e.base().what(), callback);
            return;
        }
    }

    SendOk(callback);
}

void ServiceController::GetUserSettings(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Debug("GetUserSettings request");

    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty())
    {
        GetLogger(session_id)->Error("GetUserSettings error: Empty user_id");
        SendError(k500InternalServerError, "Empty user_id", callback);
        return;
    }

    try
    {
        //update the existing json, load it, change and save
        orm::Result result = db->execSqlSync("select login, name, email, settings from users where user_id=$1", user_id);
        if (result.affectedRows() == 0)
        {
            GetLogger(session_id)->Error("Database error: Error updating settings");
            SendError(k500InternalServerError, "Error updating settings", callback);
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
            SendError(k500InternalServerError, "Json error", callback);
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
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void ServiceController::RecoverPassword(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    GetLogger(session_id)->Debug("RecoverPassword request");

    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        GetLogger(session_id)->Error("RecoverPassword error: Wrong request: Json not found in the request");
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    if (!json->isMember("password") || !(*json)["password"].isString() || !json->isMember("email_code") || !(*json)["email_code"].isString())
    {
        GetLogger(session_id)->Error("RecoverPassword error: Wrong request: empty request");
        SendError(k400BadRequest, "Wrong request: empty request", callback);
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
        SendError(k400BadRequest, "Wrong request: empty request", callback);
        return;
    }

    SessionPtr session = req->session();
#ifndef TEST
    if (session->get<std::string>("email_code") != email_code)
    {
        SendError(k400BadRequest, "Wrong email code", callback);
        return;
    }
#endif
    session->erase("email_code");

    ClearDbTurnOff t; //skip the clear db circles for a while
    orm::DbClientPtr db = app().getDbClient();
    auto email = session->get<std::string>("email_code_email");

    //generate the hash of the password with salt
    std::string salt(boost::lexical_cast<std::string>(boost::uuids::random_generator()()));
    salt.erase(std::remove(salt.begin(), salt.end(), '-'), salt.end());
    std::string hash = GetHash(password, salt);

    try
    {
        orm::Result result = db->execSqlSync("update users set password=$1 where email=$2", salt + hash, email);
        if (result.affectedRows() == 0)
        {
            GetLogger(session_id)->Error("Database error: Error updating password");
            SendError(k500InternalServerError, "Error updating password", callback);
            return;
        }
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
        return;
    }

    SendOk(callback);
}

};
