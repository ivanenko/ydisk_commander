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

#ifndef YDISK_COMMANDER_RESTCLIENT_H
#define YDISK_COMMANDER_RESTCLIENT_H

#include <string>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json11.hpp"

typedef int BOOL;

class rest_client_exception: public std::runtime_error
{
public:
    rest_client_exception(int status_code, const std::string& message):
            status_code(status_code),
            std::runtime_error(message)
    {
        std::stringstream s;
        s << "Error code: " << std::to_string(status_code);
        if(!message.empty())
            s << ";\n" << message;

        msg_ = s.str();
    }

    virtual const char* what() const throw () {
        msg_.c_str();
    }

    int get_status(){ return status_code; }

    std::string get_message(){ return std::runtime_error::what(); }

protected:
    int status_code;
    std::string msg_;
};

std::string get_yandex_oauth_token();

class YdiskRestClient final {
public:

    YdiskRestClient();
    ~YdiskRestClient();

    void set_oauth_token(const char* token);

    json11::Json get_disk_info();

    json11::Json get_resources(std::string path, BOOL isTrash);

    void makeFolder(std::string utf8Path);

    void removeResource(std::string utf8Path);

    void downloadFile(std::string path, std::ofstream &ofstream);

    void uploadFile(std::string path, std::ifstream& ifstream, BOOL overwrite);

    void move(std::string from, std::string to, BOOL overwrite);

    void copy(std::string from, std::string to, BOOL overwrite);

    void saveFromUrl(std::string urlFrom, std::string pathTo);

    // remove all files from trash
    void cleanTrash();

    // remove one file or folder from trash
    void deleteFromTrash(std::string utf8Path);

private:
    const char* token;
    httplib::SSLClient* http_client;
    httplib::Headers headers;

    void check_parsing_result(json11::Json json, std::string err);
    void throw_response_error(httplib::Response* resp);

    std::string url_encode(const std::string& s);
    std::string utf8_encode(const std::wstring& s);

    void wait_success_operation(std::string &body);
    void _do_download(std::string url, std::ofstream &ofstream);

};

#endif //YDISK_COMMANDER_RESTCLIENT_H
