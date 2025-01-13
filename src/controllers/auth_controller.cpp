#include "auth_controller.h"
#include "../logic/clear_db.h"
#include "utils.h"
#include <jwt-cpp/jwt.h>
#include <fstream>
#include <system_error>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <functional>
#include <openssl/md5.h>

namespace yutovo_server
{

//AuthController

using namespace cimg_library;

AuthController::AuthController()
{
    const Json::Value& v = app().getCustomConfig();
    session_expires = v.get("user_session_expire_timeout", 84600).asInt();
}

void AuthController::GetCaptcha(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("Get captcha request");

    // Generate captcha text (6 char max).
    const char *predef_words[] = {
        "aarrgh", "abacas", "abacus", "abakas", "abamps", "abased", "abaser", "abases", "abasia", "abated", "abater",
        "abates", "abatis", "abator", "baobab", "barbal", "barbed", "barbel", "barber", "barbes", "barbet", "barbie",
        "barbut", "barcas", "barded", "bardes", "bardic", "barege", "cavies", "cavils", "caving", "cavity", "cavort",
        "cawing", "cayman", "cayuse", "ceased", "ceases", "cebids", "ceboid", "cecity", "cedarn", "dicast", "dicers",
        "dicier", "dicing", "dicker", "dickey", "dickie", "dicots", "dictum", "didact", "diddle", "diddly", "didies",
        "didoes", "emails", "embalm", "embank", "embark", "embars", "embays", "embeds", "embers", "emblem", "embody",
        "emboli", "emboly", "embosk", "emboss", "fluffy", "fluids", "fluish", "fluked", "flukes", "flukey", "flumed",
        "flumes", "flumps", "flunks", "flunky", "fluors", "flurry", "fluted", "genome", "genoms", "genres", "genros",
        "gentes", "gentil", "gentle", "gently", "gentry", "geodes", "geodic", "geoids", "gerahs", "gerbil", "hotter",
        "hottie", "houdah", "hounds", "houris", "hourly", "housed", "housel", "houser", "houses", "hovels", "hovers",
        "howdah", "howdie", "inland", "inlays", "inlets", "inlier", "inmate", "inmesh", "inmost", "innage", "innate",
        "inners", "inning", "inpour", "inputs", "inroad", "joypop", "jubbah", "jubhah", "jubile", "judder", "judged",
        "judger", "judges", "judoka", "jugate", "jugful", "jugged", "juggle", "jugula", "knifer", "knifes", "knight",
        "knives", "knobby", "knocks", "knolls", "knolly", "knosps", "knotty", "knouts", "knower", "knowns", "knubby",
        "legate", "legato", "legend", "legers", "legged", "leggin", "legion", "legist", "legits", "legman", "legmen",
        "legong", "legume", "lehuas", "mammal", "mammas", "mammee", "mammer", "mammet", "mammey", "mammie", "mammon",
        "mamzer", "manage", "manana", "manats", "manche", "manege", "nihils", "nilgai", "nilgau", "nilled", "nimble",
        "nimbly", "nimbus", "nimmed", "nimrod", "ninety", "ninjas", "ninons", "ninths", "niobic", "offish", "offkey",
        "offset", "oftest", "ogdoad", "oghams", "ogival", "ogives", "oglers", "ogling", "ogress", "ogrish", "ogrism",
        "ohmage", "papaws", "papaya", "papers", "papery", "pappus", "papula", "papule", "papyri", "parade", "paramo",
        "parang", "paraph", "parcel", "pardah", "quasar", "quatre", "quaver", "qubits", "qubyte", "queans", "queasy",
        "queazy", "queens", "queers", "quelea", "quells", "quench", "querns", "raised", "raiser", "raises", "raisin",
        "raitas", "rajahs", "rakees", "rakers", "raking", "rakish", "rallye", "ralphs", "ramada", "ramate", "savory",
        "savour", "savoys", "sawers", "sawfly", "sawing", "sawlog", "sawney", "sawyer", "saxony", "sayeds", "sayers",
        "sayest", "sayids", "tondos", "toneme", "toners", "tongas", "tonged", "tonger", "tongue", "tonics", "tonier",
        "toning", "tonish", "tonlet", "tonner", "tonnes", "uredia", "uredos", "ureide", "uremia", "uremic", "ureter",
        "uretic", "urgent", "urgers", "urging", "urials", "urinal", "urines", "uropod", "villus", "vimina", "vinals",
        "vincas", "vineal", "vinery", "vinier", "vinify", "vining", "vinous", "vinyls", "violas", "violet", "violin",
        "webfed", "weblog", "wechts", "wedded", "wedder", "wedeln", "wedels", "wedged", "wedges", "wedgie", "weeded",
        "weeder", "weekly", "weened", "xystoi", "xystos", "xystus", "yabber", "yabbie", "yachts", "yacked", "yaffed",
        "yagers", "yahoos", "yairds", "yakked", "yakker", "yakuza", "zigged", "zigzag", "zillah", "zinced", "zincic",
        "zincky", "zinebs", "zinged", "zinger", "zinnia", "zipped", "zipper", "zirams", "zircon"};
    cimg::srand();
    const char *const captcha_text = predef_words[std::rand() % (sizeof(predef_words) / sizeof(char *))];

    // Create captcha image
    // Write colored and distorted text
    CImg<unsigned char> captcha(256, 64, 1, 3, 0), color(3);
    char letter[2] = {0};
    for (unsigned int k = 0; k < 6; ++k)
    {
        CImg<unsigned char> tmp;
        *letter = captcha_text[k];
        if (*letter)
        {
            cimg_forX(color, i) color[i] = (unsigned char)(128 + (std::rand() % 127));
            tmp.draw_text((int)(2 + 8 * cimg::rand()), (int)(12 * cimg::rand()), letter, color.data(), 0, 1, std::rand() % 2 ? 38 : 57).resize(-100, -100, 1, 3);
            const unsigned int dir = std::rand() % 4, wph = tmp.width() + tmp.height();
            cimg_forXYC(tmp, x, y, v)
            {
                const int val = dir == 0 ? x + y : (dir == 1 ? x + tmp.height() - y : (dir == 2 ? y + tmp.width() - x : tmp.width() - x + tmp.height() - y));
                tmp(x, y, v) = (unsigned char)std::max(0.0f, std::min(255.0f, 1.5f * tmp(x, y, v) * val / wph));
            }
            if (std::rand() % 2)
                tmp = (tmp.get_dilate(3) -= tmp);
            tmp.blur((float)cimg::rand() * 0.8f).normalize(0, 255);
            const float sin_offset = (float)cimg::rand(-1, 1) * 3, sin_freq = (float)cimg::rand(-1, 1) / 7;
            cimg_forYC(captcha, y, v) captcha.get_shared_row(y, 0, v).shift((int)(4 * std::cos(y * sin_freq + sin_offset)));
            captcha.draw_image(6 + 40 * k, tmp);
        }
    }

    // Add geometric and random noise
    CImg<unsigned char> copy = (+captcha).fill(0);
    for (unsigned int l = 0; l < 3; ++l)
    {
        if (l)
            copy.blur(0.5f).normalize(0, 148);
        for (unsigned int k = 0; k < 10; ++k)
        {
            cimg_forX(color, i) color[i] = (unsigned char)(128 + cimg::rand() * 127);
            if (cimg::rand() < 0.5f)
                copy.draw_circle((int)(cimg::rand() * captcha.width()), (int)(cimg::rand() * captcha.height()), (int)(cimg::rand() * 30), color.data(), 0.6f, ~0U);
            else
                copy.draw_line((int)(cimg::rand() * captcha.width()), (int)(cimg::rand() * captcha.height()), (int)(cimg::rand() * captcha.width()),
                    (int)(cimg::rand() * captcha.height()), color.data(), 0.6f);
        }
    }
    captcha |= copy;
    captcha.noise(10, 2);
    captcha = (+captcha).fill(255) - captcha;

    SessionPtr session = req->session();
    session->erase("captcha");
    session->insert("captcha", std::string(captcha_text));

    SendCaptcha(callback, captcha);

    GetLogger(session_id)->Info("Sent captcha: {}", captcha_text);
}

void AuthController::Register(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, User&& user)
{
    std::string session_id = req->getCookie("session_id");
    GetLogger(session_id)->Info("Register request: login={}, email={}, password={}, name={}", user.login, user.email, user.password, user.name);

    SessionPtr session = req->session();
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

#ifndef TEST
    auto captcha = (*json)["captcha"].asString();
    if (captcha.empty() || session->get<std::string>("captcha") != captcha)
    {
        SendError(k400BadRequest, "Wrong captcha", callback);
        return;
    }
#endif

    orm::DbClientPtr db = app().getDbClient();
    if (user.login.empty() || user.email.empty() || user.password.empty())
    {
        SendError(k400BadRequest, "Fields must not be empty", callback);
        return;
    }

    try
    {
        //check if such login already exists
        orm::Result result = db->execSqlSync("select 1 from users where login=$1", user.login);
        if (result.size() > 0)
        {
            SendError(k409Conflict, "Login already exists", callback);
            return;
        }

        //generate the hash of the password with salt
        std::string salt(boost::lexical_cast<std::string>(boost::uuids::random_generator()()));
        salt.erase(std::remove(salt.begin(), salt.end(), '-'), salt.end());
        std::string hash = GetHash(user.password, salt);

        //insert new user
        result = db->execSqlSync("insert into users (login, password, email, name) values ($1, $2, $3, $4)", user.login, salt + hash, user.email, user.name);
        if (result.size() == 0)
        {
            SendOk(callback);
        }
        else
        {
            GetLogger(req->getCookie("session_id"))->Error("Database error: {}", "Error of insert");
            SendError(k500InternalServerError, "Error of insert", callback);
        }
    } 
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(req->getCookie("session_id"))->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void AuthController::UnRegister(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id = req->getCookie("session_id");
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    if (!json->isMember("login") || !(*json)["login"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", callback);
        return;
    }

    auto login = (*json)["login"].asString();
    SessionPtr session = req->session();
    GetLogger(session_id)->Info("UnRegister request: login={}", login);
    orm::DbClientPtr db = app().getDbClient();
    std::string user_id = session->get<std::string>("user_id");
    if (user_id.empty() || user_id == "-1")
    {
        SendError(k400BadRequest, "User not found", callback);
        return;
    }

    try
    {
        orm::Result result = db->execSqlSync("delete from users where login=$1", login);
        if (result.affectedRows() == 0)
        {
            SendError(k404NotFound, "Login not found", callback);
            return;
        }
        db->execSqlSync("delete from user_sessions where user_id=$1", user_id);
        session->erase("user_id");
        SendOk(callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void AuthController::Login(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    session_id = req->getCookie("session_id");
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    if (!json->isMember("login") || !(*json)["login"].isString() || !json->isMember("password") || !(*json)["password"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", callback);
        return;
    }

    SessionPtr session = req->session();

    auto login = (*json)["login"].asString();
    auto password = (*json)["password"].asString();
    bool load_last = true;
    if (json->isMember("load_last") && (*json)["load_last"].isBool())
        load_last = (*json)["load_last"].asBool();

#ifndef TEST
    auto captcha = (*json)["captcha"].asString();
    if (captcha.empty() || session->get<std::string>("captcha") != captcha)
    {
        SendError(k400BadRequest, "Wrong captcha", callback);
        return;
    }
#endif

    logger->Info("Login request: login={}, load_last={}", login, load_last);
    orm::DbClientPtr db = app().getDbClient();

    std::string user_id;
    try
    {
        ClearDbTurnOff t; //skip the clear db circles for a while

        orm::Result result = db->execSqlSync("select user_id, password, language, settings from users where login=$1", login);
        if (result.size() == 0)
        {
            SendError(k401Unauthorized, "Login or password are incorrect", callback);
            return;
        }

        auto row = result[0];
        std::string hash = row["password"].as<std::string>();
        std::string salt = hash.substr(0, 32);
        std::string h = GetHash(password, salt);
        if (salt + h != hash)
        {
            SendError(k401Unauthorized, "Login or password are incorrect", callback);
            return;
        }

        //create a session with refresh and access tokens
        session->erase("login");
        session->insert("login", login);

        std::string refresh_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
        std::string access_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
        trantor::Date access_expires = trantor::Date::now().after(access_token_expires);
        trantor::Date refresh_expires = trantor::Date::now().after(refresh_token_expires);

        user_id = row["user_id"].as<std::string>();
        result = db->execSqlSync("insert into refresh_sessions (user_id, refresh_uuid, expire_time) values ($1, $2, $3)", 
            user_id, refresh_uuid, refresh_expires.secondsSinceEpoch());
        session->erase("user_id");
        session->insert("user_id", user_id);

        std::string document_id = "-1";
        std::string name;
        session->insert("session_id", session_id);
        auto session_expires_date = trantor::Date::now().after(session_expires);
        if (!session_id.empty())
        {
            if (load_last)
            {
                result = db->execSqlSync("select document_id from user_sessions where session_id=$1", session_id);
                if (result.size() > 0)
                {
                    auto row = result[0];
                    document_id = row["document_id"].as<std::string>();
                }
            }
            if (document_id == "-1")
            {
                int d = GetFirstEmptyDocument(user_id);
                if (d == -1)
                {
                    if (!AddDocument(user_id, document_id, name))
                    {
                        GetLogger(session_id)->Error("Database error: Error inserting a document");
                        SendError(k500InternalServerError, "Error inserting a document", callback);
                        return;
                    }
                }
                else
                    document_id = std::to_string(d);
            }

            //if a user has many logins, a session may have another login
            db->execSqlSync("delete from user_sessions where session_id=$1", session_id);
            //the user session starts to have an owner
            db->execSqlSync("insert into user_sessions (session_id, user_id, expire_time, document_id) values ($1, $2, $3, $4)", 
                session_id, user_id, session_expires_date.secondsSinceEpoch(), document_id);
        }

        std::string language = row["language"].as<std::string>();
        std::string settings = row["settings"].as<std::string>();

        result = db->execSqlSync("select max_files, max_solving_time, max_file_size from user_plans where plan_id=(select plan_id from users where user_id=$1)", user_id);
        if (result.size() == 0)
        {
            SendError(k500InternalServerError, "User plan is incorrect", callback);
            return;
        }
        auto r = result[0];
        session->insert("max_solving_time", r["max_solving_time"].as<int>());
        session->insert("max_file_size", r["max_file_size"].as<int>() * 1024);
        session->insert("max_files", r["max_files"].as<int>());

        SendOkTokens(callback, login, access_uuid, refresh_uuid, session_id, access_expires, refresh_expires, session_expires_date, 
            document_id, name, language, settings);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(req->getCookie("session_id"))->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }

    session->erase("captcha");
    auth_logger->Info("Login: login={}, user_id={}", login, user_id);
}

void AuthController::Logout(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    std::string refresh_token = req->getCookie("refresh_token");
    std::string session_id = session->get<std::string>("session_id");

    if (!json->isMember("login") || !(*json)["login"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", callback);
        return;
    }

    auto login = (*json)["login"].asString();
    std::string refresh_uuid;
    if (!ParseRefreshToken(refresh_token, refresh_uuid, login, callback))
        return;

    GetLogger(session_id)->Info("Logout request: login={}", login);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        orm::Result result = db->execSqlSync("update user_sessions set user_id=-1 where session_id=$1", session_id);
        result = db->execSqlSync("delete from refresh_sessions where user_id=$1 and refresh_uuid=$2", user_id, refresh_uuid);
        if (result.affectedRows() > 0)
            SendOk(callback);
        else
            SendError(k401Unauthorized, "Login or password are incorrect", callback);
        session->erase("user_id");
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }

    auth_logger->Info("Logout: login={}, user_id={}", login, user_id);
}

void AuthController::RefreshToken(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    std::string session_id = req->getCookie("session_id");
    std::string refresh_token = req->getCookie("refresh_token");
    orm::DbClientPtr db = app().getDbClient();
    SessionPtr session = req->session();

    try
    {
        std::string refresh_uuid;
        std::string login;
        if (!ParseRefreshToken(refresh_token, refresh_uuid, login, callback))
            return;

        GetLogger(session_id)->Info("RefreshToken request: login={}, refresh_uuid={}", login, refresh_uuid);

        orm::Result result = db->execSqlSync("select user_id from users where login=$1", login);
        if (result.size() == 0)
        {
            SendError(k401Unauthorized, "User not found", callback);
            return;
        }

        auto row = result[0];
        auto user_id = row["user_id"].as<std::string>();

        result = db->execSqlSync("delete from refresh_sessions where refresh_uuid=$1", refresh_uuid);
        if (result.affectedRows() == 0)
        {
            SendError(k401Unauthorized, "Refresh session is incorrect", callback);
            return;
        }

        refresh_uuid = std::string(boost::uuids::to_string(boost::uuids::random_generator()()));
        std::string access_uuid(boost::uuids::to_string(boost::uuids::random_generator()()));
        trantor::Date access_expires = trantor::Date::now().after(access_token_expires);
        trantor::Date refresh_expires = trantor::Date::now().after(refresh_token_expires);

        result = db->execSqlSync("insert into refresh_sessions (user_id, refresh_uuid, expire_time) values ($1, $2, $3)", 
            user_id, refresh_uuid, refresh_expires.secondsSinceEpoch());
        
        session->insert("login", login);
        session->erase("user_id");
        session->insert("user_id", user_id);

        std::string session_id = req->getCookie("session_id");
        if (!session_id.empty())
            UpdateSessionTime(session_id);

        if (user_id != "-1" && session->get<int>("max_file_size") == 0)
        {
            result = db->execSqlSync("select max_files, max_solving_time, max_file_size from user_plans where plan_id=(select plan_id from users where user_id=$1)", user_id);
            if (result.size() == 0)
            {
                GetLogger(session_id)->Error("Database error: user plan not found");
                SendError(k500InternalServerError, "User plan not found", callback);
                return;
            }
            auto row = result[0];
            session->insert("max_solving_time", row["max_solving_time"].as<int>());
            session->insert("max_file_size", row["max_file_size"].as<int>() * 1024);
            session->insert("max_files", row["max_files"].as<int>());
        }

        session->insert("session_id", session_id);

        auto session_expires_date = trantor::Date::now().after(session_expires);
        SendOkTokens(callback, login, access_uuid, refresh_uuid, session_id, access_expires, refresh_expires, session_expires_date);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void AuthController::SetLanguage(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    auto json = req->getJsonObject();
    if (!json || !json->isObject())
    {
        SendError(k400BadRequest, "Json not found in the request", callback);
        return;
    }

    std::string session_id = req->getCookie("session_id");
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");

    if (!json->isMember("language") || !(*json)["language"].isString())
    {
        SendError(k400BadRequest, "Wrong json in the request", callback);
        return;
    }

    auto language = (*json)["language"].asString();

    GetLogger(session_id)->Info("Set language request: user_id={}, language={}", user_id, language);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        orm::Result result = db->execSqlSync("update users set language=$1 where user_id=$2", language, user_id);
        if (result.affectedRows() > 0)
            SendOk(callback);
        else
            SendError(k401Unauthorized, "Login or password are incorrect", callback);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

void AuthController::GetLanguage(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    SessionPtr session = req->session();
    std::string user_id = session->get<std::string>("user_id");
    std::string session_id = req->getCookie("session_id");

    GetLogger(session_id)->Info("Get language request: user_id={}", user_id);
    orm::DbClientPtr db = app().getDbClient();

    try
    {
        ClearDbTurnOff t; //skip the clear db circles for a while

        orm::Result result = db->execSqlSync("select language from users where user_id=$1", user_id);
        if (result.size() == 0)
        {
            SendError(k401Unauthorized, "Login or password are incorrect", callback);
            return;
        }

        auto row = result[0];
        auto language = row["language"].as<std::string>();

        Json::Value v(Json::objectValue);
        v["language"] = language;
        SendJson(callback, v);
    }
    catch (const orm::DrogonDbException& e)
    {
        GetLogger(session_id)->Error("Database error: {}", e.base().what());
        SendError(k500InternalServerError, e.base().what(), callback);
    }
}

#ifdef TEST
void AuthController::SetParams(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    access_token_expires = req->getOptionalParameter<int>("access_token_expires").value();
    refresh_token_expires = req->getOptionalParameter<int>("refresh_token_expires").value();
    SendOk(callback);
}
#endif

void AuthController::UpdateSessionTime(const std::string& session_id)
{
    orm::DbClientPtr db = app().getDbClient();
    trantor::Date session_expires_date = trantor::Date::now().after(session_expires);
    db->execSqlSync("update user_sessions set expire_time=$1 where session_id=$2", session_expires_date.secondsSinceEpoch(), session_id);
}

std::string AuthController::GetHash(const std::string& str, const std::string& salt)
{
    unsigned char hash[MD5_DIGEST_LENGTH];
    std::string s = str + salt;
    MD5((const unsigned char*)s.c_str(), s.size(), hash);
    char hash_str[MD5_DIGEST_LENGTH * 2];
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
        sprintf(&hash_str[i * 2], "%02x", (unsigned int)hash[i]);
    return std::string(&hash_str[0], MD5_DIGEST_LENGTH * 2);
}

}
