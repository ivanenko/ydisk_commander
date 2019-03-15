/*
Wfx plugin for working with Yandex Disk storage

Copyright (C) 2019 Ivanenko Danil (ivanenko.danil@gmail.com)

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
        License as published by the Free Software Foundation; either
        version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
        Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
        Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <codecvt>
#include <future>
#include "restclient.h"

using namespace json11;


void _listen_server(httplib::Server* svr,  std::promise<std::string> *pr){

    svr->Get("/get_token", [pr](const httplib::Request& req, httplib::Response& res) {
        std::string html = R"(
            <html><body>
                <h2 id="message">Success</h2>
                <script type="text/javascript">
                    var hash = window.location.hash.substr(1);
                    var i = hash.search("access_token=");
                    if(i<0){
                        document.getElementById("message").textContent = "Error! Cannot get auth_token";
                    } else {
                        var token = hash.substr(i)
                                        .split("&")[0]
                                        .split("=")[1];

                        if(token){
                            var xhttp = new XMLHttpRequest();
                            xhttp.onreadystatechange = function() {
                                if (this.readyState == 4 && this.status == 200) {
                                 document.getElementById("message").textContent = "Success! You can close browser and use DC plugin";
                                }
                             };
                            xhttp.open("GET", "/receive_token?access_token="+token, true);
                            xhttp.send();
                        }
                    }
                </script>
            </body></html>
        )";

        res.set_content(html, "text/html");
    });

    svr->Get("/receive_token", [pr](const httplib::Request& req, httplib::Response& res) {
        std::string token = req.get_param_value("access_token");
        res.set_content("OAuth token received", "text/plain");
        pr->set_value(token);
    });

    //int port = svr->bind_to_any_port("localhost");
    //std::cout << "Starting server at port: " << port << std::endl;
    //svr->listen_after_bind();
    svr->listen("localhost", 3359);
}

std::string get_yandex_oauth_token()
{
    httplib::Server server;
    std::string token;

    std::promise<std::string> promiseToken;
    std::future<std::string> ftr = promiseToken.get_future();

    //create and run server on separate thread
    std::thread server_thread(_listen_server, &server, &promiseToken);

    //TODO add win32 browser open
    system("xdg-open https://oauth.yandex.ru/authorize?client_id=bc2f272cc37349b7a1320b9ac7826ebf\\&response_type=token");

    try{
        std::future_status ftr_status = ftr.wait_for(std::chrono::seconds(20));
        if(ftr_status == std::future_status::ready) {
            token = ftr.get();
        } else if (ftr_status == std::future_status::timeout){
            //std::cout << "Timeout!!! token is unknown\n";
        } else {
            //std::cout << "Future status = deffered\n";
        }

    } catch(std::exception & e) {
        //std::cout << e.what() << std::endl;
    }

    server.stop();
    server_thread.join();

    return token;
}


YdiskRestClient::YdiskRestClient()
{
    http_client = new httplib::SSLClient("cloud-api.yandex.net");
}

YdiskRestClient::~YdiskRestClient()
{
    delete http_client;
}

void YdiskRestClient::set_oauth_token(const char *token)
{
    assert(token != NULL);
    this->token = token;

    std::string header_token = "OAuth ";
    header_token += token;
    headers = { {"Authorization", header_token} };
}

void YdiskRestClient::throw_response_error(httplib::Response* resp){
    if(resp){
        std::string error, message;
        auto json = Json::parse(resp->body, error);

        if(!json.is_null() && !json["description"].is_null())
            message = json["description"].string_value();
        else
            message = resp->body;

        throw rest_client_exception(resp->status, message);
    } else {
        throw std::runtime_error("Unknown error");
    }
}

Json YdiskRestClient::get_disk_info()
{
    auto r = http_client->Get("/v1/disk/", headers);

    if(r.get() && r->status==200){
        std::string error;
        auto json = Json::parse(r->body, error);
        check_parsing_result(json, error);
        return json;
    } else {
        throw_response_error(r.get());
    }
}

Json YdiskRestClient::get_resources(std::string path, BOOL isTrash)
{
    //TODO turn off unnesesary fields
    std::string url("/v1/disk/");
    if(isTrash)
        url += "trash/";
    url += "resources?limit=1000&path=";
    url += url_encode(path);
    auto r = http_client->Get(url.c_str(), headers);

    if(r.get() && r->status==200){
        std::string error;
        const auto json = Json::parse(r->body, error);
        check_parsing_result(json, error);

        //TODO check if total > limit
        return json;
    } else {
        throw_response_error(r.get());
    }
}

void YdiskRestClient::makeFolder(std::string utf8Path)
{
    std::string url("/v1/disk/resources?path=");
    url += url_encode(utf8Path);
    std::string empty_body;
    auto r = http_client->Put(url.c_str(), headers, empty_body, "text/plain");

    if(!r.get() || r->status!=201)
        throw_response_error(r.get());
}

void YdiskRestClient::removeResource(std::string utf8Path)
{
    std::string url("/v1/disk/resources?path=");
    url += url_encode(utf8Path);
    auto r = http_client->Delete(url.c_str(), headers);

    if(r.get() && r->status==204){
        //empty folder or file was removed
        return;
    } else if(r.get() && r->status==202){
        //not empty folder is removing, get operation status
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YdiskRestClient::downloadFile(std::string path, std::ofstream &ofstream)
{
    std::string url("/v1/disk/resources/download?path=");
    url += url_encode(path);

    //Get download link
    auto r = http_client->Get(url.c_str(), headers);
    if(r.get() && r->status==200){
        std::string error;
        const auto json = Json::parse(r->body, error);
        check_parsing_result(json, error);
        if(!json["href"].is_string())
            throw std::runtime_error("Wrong json format");

        url = json["href"].string_value();
    } else {
        throw_response_error(r.get());
    }

    _do_download(url, ofstream);

}

void YdiskRestClient::_do_download(std::string url, std::ofstream &ofstream){
    std::string server_url, request_url;

    std::smatch m;
    auto pattern = std::regex("https://(.+?)(/.+)");
    if (std::regex_search(url, m, pattern)) {
        server_url = m[1].str();
        request_url = m[2].str();
    } else {
        throw std::runtime_error("Error parsing file href for download");
    }

    httplib::SSLClient cli2(server_url.c_str());
    auto r2 = cli2.Get(request_url.c_str());
    if(r2.get() && r2->status == 200){
        ofstream.write(r2->body.data(), r2->body.size());
    } else if(r2.get() && r2->status == 302){ //redirect
        if(r2->has_header("Location"))
            _do_download(r2->get_header_value("Location"), ofstream);
        else
            throw std::runtime_error("Cannot find redirect link");
    } else {
        throw_response_error(r2.get());
    }
}

void YdiskRestClient::uploadFile(std::string path, std::ifstream& ifstream, BOOL overwrite)
{
    std::string url("/v1/disk/resources/upload?path=");
    url += url_encode(path);
    url += "&overwrite=";
    url += (overwrite ? "true": "false");

    auto r = http_client->Get(url.c_str(), headers);
    if(r.get() && r->status==200){
        std::string error;
        const auto json = Json::parse(r->body, error);
        url = json["href"].string_value();
    } else {
        throw_response_error(r.get());
    }

    std::stringstream body;
    body << ifstream.rdbuf();

    std::string server_url, request_url;
    std::smatch m;
    auto pattern = std::regex("https://(.+?):443(/.+)");
    if (std::regex_search(url, m, pattern)) {
        server_url = m[1].str();
        request_url = m[2].str();
    } else {
        throw std::runtime_error("Error parsing file href for upload");
    }

    httplib::SSLClient cli2(server_url.c_str(), 443);
    auto r2 = cli2.Put(request_url.c_str(), headers, body.str(), "application/octet-stream");

    if(r2.get() && (r2->status==201 || r2->status==202)){
        return;
    } else {
        throw_response_error(r2.get());
    }
}

void YdiskRestClient::move(std::string from, std::string to, BOOL overwrite)
{
    std::string url("/v1/disk/resources/move?from=");
    url += url_encode(from) + "&path=" + url_encode(to);
    url += "&overwrite=";
    url += (overwrite == true ? "true": "false");
    std::string empty_body;
    auto r = http_client->Post(url.c_str(), headers, empty_body, "text/plain");

    if(r.get() && r->status==201){
        //success
        return;
    } else if(r.get() && r->status==202){
        //not empty folder is moving, get operation status
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YdiskRestClient::copy(std::string from, std::string to, BOOL overwrite)
{
    //TODO - max 'to' path is the 32760 symbols
    std::string url("/v1/disk/resources/copy?from=");
    url += url_encode(from) + "&path=" + url_encode(to);
    url += "&overwrite=";
    url += (overwrite == true ? "true": "false");
    std::string empty_body;
    auto r = http_client->Post(url.c_str(), headers, empty_body, "text/plain");

    if(r.get() && r->status==201){
        //success
        return;
    } else if(r.get() && r->status==202){
        //not empty folder is copying, get operation status
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YdiskRestClient::saveFromUrl(std::string urlFrom, std::string pathTo)
{
    std::string url("/v1/disk/resources/upload?url=");
    url += url_encode(urlFrom) + "&path=" + url_encode(pathTo);
    std::string empty_body;
    auto r = http_client->Post(url.c_str(), headers, empty_body, "text/plain");

    if(r.get() && r->status==202){
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YdiskRestClient::wait_success_operation(std::string &body)
{
    std::string error;
    const auto json = Json::parse(body, error);
    check_parsing_result(json, error);
    if(!json["href"].is_string())
        throw std::runtime_error("Getting operation status error: unknown json format");

    int pos = json["href"].string_value().find("/v1/disk");
    std::string url_status = json["href"].string_value().substr(pos);
    BOOL success = false;
    int waitCount = 0;
    do{
        std::this_thread::sleep_for(std::chrono::seconds(2));
        auto r2 = http_client->Get(url_status.c_str(), headers);
        if(r2.get() && r2->status==200){
            std::string err;
            const auto json2 = Json::parse(r2->body, err);
            check_parsing_result(json2, err);
            if(!json2["status"].is_string())
                throw std::runtime_error("Getting operation status error: unknown json format");

            if(json2["status"].string_value() == "success")
                success = true;

            if(json2["status"].string_value() == "failure")
                throw rest_client_exception(500, "Operation error");

            // if(json2["status"].string_value() == "in-progress") - Do nothing, just wait
            waitCount++;
        } else {
            throw std::runtime_error("Error getting operation status");
        }

    } while(success==false && waitCount<30); //wait at least 1 min

    if(success==false && waitCount>=30)
        throw std::runtime_error("Operation is too long");

}

void YdiskRestClient::check_parsing_result(Json json, std::string err)
{
    if(json.is_null() || !err.empty())
        throw std::runtime_error("Error parsing json response");
}

std::string YdiskRestClient::utf8_encode(const std::wstring& s)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    return convert.to_bytes(s);
}

std::string YdiskRestClient::url_encode(const std::string& s)
{
    std::string result;

    for (auto i = 0; s[i]; i++) {
        switch (s[i]) {
            case ' ':  result += "%20"; break;
            case '+':  result += "%2B"; break;
            case '/':  result += "%2F"; break;
            case '\'': result += "%27"; break;
            case ',':  result += "%2C"; break;
            case ':':  result += "%3A"; break;
            case ';':  result += "%3B"; break;
            case '#':  result += "%23"; break;
            default:
                auto c = static_cast<uint8_t>(s[i]);
                if (c >= 0x80) {
                    result += '%';
                    char hex[4];
                    size_t len = snprintf(hex, sizeof(hex) - 1, "%02X", c);
                    assert(len == 2);
                    result.append(hex, len);
                } else {
                    result += s[i];
                }
                break;
        }
    }

    return result;
}

void YdiskRestClient::cleanTrash()
{
    std::string url("/v1/disk/trash/resources");
    auto r = http_client->Delete(url.c_str(), headers);

    if(r.get() && r->status==204){
        return;
    } else if(r.get() && r->status==202){
        //not empty folder is removing, get operation status
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}

void YdiskRestClient::deleteFromTrash(std::string utf8Path)
{
    std::string url("/v1/disk/trash/resources?path=");
    url += url_encode(utf8Path);
    auto r = http_client->Delete(url.c_str(), headers);

    if(r.get() && r->status==204){
        return;
    } else if(r.get() && r->status==202){
        //not empty folder is removing, get operation status
        wait_success_operation(r->body);
        return;
    } else {
        throw_response_error(r.get());
    }
}
