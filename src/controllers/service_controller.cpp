#include "service_controller.h"
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
    get_files(fs::path(tasks_path), root);

    SendJson(callback, root);
}

void ServiceController::LoadTask(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    auto json = req->getJsonObject();
    auto task = (*json)["task"].asString();
    logger->Info("LoadTask request task={}", task);
    task = tasks_path + task + ".yut";
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

};
