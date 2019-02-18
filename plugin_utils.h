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

#ifndef YDISK_COMMANDER_PLUGIN_UTILS_H
#define YDISK_COMMANDER_PLUGIN_UTILS_H

#include <string>
#include <ctime>
#include "common.h"

const char kPathSeparator =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif

typedef std::basic_string<WCHAR> wcharstring;

typedef struct {
    int nCount;
    int nSize;
    WIN32_FIND_DATAW* resource_array;
} tResources, *pResources;


FILETIME parse_iso_time(std::string time){
    struct tm t, tz;
    strptime(time.c_str(), "%Y-%m-%dT%H:%M:%S%z", &t);
    long int gmtoff = t.tm_gmtoff;
    time_t t2 = mktime(&t) + gmtoff; //TODO gmtoff doesnt correct here
    int64_t ft = (int64_t)t2 * 10000000 + 116444736000000000;
    FILETIME file_time;
    file_time.dwLowDateTime = ft & 0xffff;
    file_time.dwHighDateTime = ft >> 32;
    return file_time;
}

// convert 2-bytes string to UTF8 encoded string
std::string toUTF8(const WCHAR *p)
{
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert2;
    return convert2.to_bytes(std::u16string((char16_t*) p));
}

// convert UTF8 to 2-bytes string
wcharstring fromUTF8(const std::string &str)
{
    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert2;
    std::u16string utf16 = convert2.from_bytes(str);
    return wcharstring((WCHAR*)utf16.data());
}

BOOL file_exists(const std::string& filename)
{
    struct stat buf;
    return stat(filename.c_str(), &buf) == 0;
}

json11::Json read_config_file(std::string& path)
{
    std::ifstream ifs;
    ifs.open(path, std::ios::binary | std::ofstream::in);
    if (!ifs || ifs.bad())
        return json11::Json();

    std::stringstream body;
    body << ifs.rdbuf();
    ifs.close();

    std::string error;
    json11::Json json = json11::Json::parse(body.str(), error);
    return json;
}

void save_config_file(const std::string& path, const char* get_token_method, const char* save_type, const char* token)
{
    json11::Json json;
    json11::Json::object json_obj = json.object_items();
    json_obj["get_token_method"] = get_token_method;
    json_obj["save_type"] = save_type;
    if(token)
        json_obj["oauth_token"] = token;

    std::ofstream ofs;
    ofs.open(path, std::ofstream::out | std::ios::trunc);
    ofs << json11::Json(json_obj).dump();

    ofs.close();
}

std::string get_oauth_token(std::string config_path, int pluginNumber, tRequestProcW requestProc, int cryptoNumber,
                            tCryptProcW cryptProc)
{
    std::string oauth_token;

    json11::Json json = read_config_file(config_path);

    // config does not exists or error parsing file
    if(json.is_null()) {
        oauth_token = get_yandex_oauth_token();
        save_config_file(config_path, "oauth", "dont_save", NULL);
        return oauth_token;
    }

    // json format error
    if(!json["save_type"].is_string() || !json["get_token_method"].is_string()){
        oauth_token = get_yandex_oauth_token();
        save_config_file(config_path, "oauth", "dont_save", NULL);
        return oauth_token;
    }


    if(json["get_token_method"].string_value() == "oauth"){
        if(json["save_type"].string_value() == "dont_save"){
            oauth_token = get_yandex_oauth_token();
            return oauth_token;
        }

        if(json["save_type"].string_value() == "config"){
            if(json["oauth_token"].is_string() && !json["oauth_token"].string_value().empty()) {
                oauth_token = json["oauth_token"].string_value();
            } else {
                oauth_token = get_yandex_oauth_token();
                save_config_file(config_path, "oauth", "config", oauth_token.c_str());
            }
            return oauth_token;
        }

        if(json["save_type"].string_value() == "password_manager"){
            WCHAR psw[MAX_PATH];
            int res = cryptProc(pluginNumber, cryptoNumber, FS_CRYPT_LOAD_PASSWORD, (WCHAR*)u"yandex_disk", psw, MAX_PATH);
            if(res != FS_FILE_OK){
                oauth_token = get_yandex_oauth_token();
                wcharstring psw2 = fromUTF8(oauth_token);
                cryptProc(pluginNumber, cryptoNumber, FS_CRYPT_SAVE_PASSWORD, (WCHAR*)u"yandex_disk", (WCHAR*)psw2.c_str(), psw2.size()+1);
            } else {
                oauth_token = toUTF8(psw);
            }

            return oauth_token;
        }
    }


    if(json["get_token_method"].string_value() == "manual"){
        if(json["save_type"].string_value() == "dont_save"){
            WCHAR psw[MAX_PATH];
            psw[0] = 0;
            BOOL res = requestProc(pluginNumber, RT_Other, (WCHAR*)u"Enter oauth token", (WCHAR*)u"Enter oauth token", psw, MAX_PATH);
            if(res > 0)
                oauth_token = toUTF8(psw);

            return oauth_token;
        }

        if(json["save_type"].string_value() == "config"){
            if(json["oauth_token"].is_string() && !json["oauth_token"].string_value().empty()) {
                oauth_token = json["oauth_token"].string_value();
            } else {
                WCHAR psw[MAX_PATH];
                psw[0] = 0;
                BOOL res = requestProc(pluginNumber, RT_Other, (WCHAR*)u"Enter oauth token", (WCHAR*)u"Enter oauth token", psw, MAX_PATH);
                if(res != 0){
                    oauth_token = toUTF8(psw);
                    save_config_file(config_path, "manual", "config", oauth_token.c_str());
                }
            }
            return oauth_token;
        }

        if(json["save_type"].string_value() == "password_manager"){
            WCHAR psw[MAX_PATH];
            int res = cryptProc(pluginNumber, cryptoNumber, FS_CRYPT_LOAD_PASSWORD, (WCHAR*)u"yandex_disk", psw, MAX_PATH);
            if(res != FS_FILE_OK){
                WCHAR psw2[MAX_PATH];
                psw2[0] = 0;
                BOOL res2 = requestProc(pluginNumber, RT_Other, (WCHAR*)u"Enter oauth token", (WCHAR*)u"Enter oauth token", psw2, MAX_PATH);
                if(res2 != 0){
                    oauth_token = toUTF8(psw2);
                    cryptProc(pluginNumber, cryptoNumber, FS_CRYPT_SAVE_PASSWORD, (WCHAR*)u"yandex_disk", psw2, MAX_PATH);
                }
            } else {
                oauth_token = toUTF8(psw);
            }

            return oauth_token;
        }
    }

    return oauth_token;
}

//std::string get_from_config(std::string &path, int pluginNumber, int gCryptoNr, tCryptProcW gCryptProcW)
//{
//    json11::Json json = read_config_file(path);
//    if(json.is_null())
//        return std::string();
//
//    if(!json["save_type"].is_string())
//        return std::string();
//
//    if(json["save_type"].string_value() == "config"){
//        if(json["oauth_token"].is_string())
//            return json["oauth_token"].string_value();
////        else
////            throw exception
//    }
//
//    if(json["save_type"].string_value() == "password_manager"){
//        WCHAR psw[MAX_PATH];
//        int res = gCryptProcW(pluginNumber, gCryptoNr, FS_CRYPT_LOAD_PASSWORD, (WCHAR*)u"yandex_disk", psw, MAX_PATH);
//        if(res != FS_FILE_OK)
//            return std::string();
//
//        std::string strResult = toUTF8(psw);
//        return strResult;
//    }
//
//    if(json["save_type"].string_value() == "dont_save"){
//        if(json["get_token_method"].string_value() == "manual"){
//            //show dialog here
//            // gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*)u"Not supported command", NULL, 0);
//        }
//    }
//
//    return std::string();
//}
//
//void save_to_config(std::string &path, std::string &oauth_token, int pluginNumber, int gCryptoNr, tCryptProcW gCryptProcW)
//{
//    json11::Json json = read_config_file(path);
//    if(json.is_null())
//        return;
//
//    if(!json["save_type"].is_string())
//        return;
//
//    if(json["save_type"].string_value() == "config"){
//        json11::Json::object json_obj = json.object_items();
//        json_obj["oauth_token"] = oauth_token;
//
//        std::ofstream ofs;
//        ofs.open(path, std::ofstream::out | std::ios::trunc);
//        //ofs.write(config.data(), config.size());
//        ofs << json11::Json(json_obj).dump();
//
//        ofs.close();
//    }
//
//    if(json["save_type"].string_value() == "password_manager"){
//        wcharstring wPassword = fromUTF8(oauth_token);
//        int res = gCryptProcW(pluginNumber, gCryptoNr, FS_CRYPT_SAVE_PASSWORD, (WCHAR*)u"yandex_disk", (WCHAR*)wPassword.c_str(), wPassword.size());
////        if(res != FS_FILE_OK)
////            return;
//    }
//
//}

wcharstring prepare_disk_info(json11::Json json)
{
    if(json.is_null())
        throw std::runtime_error("Json is null");

    std::stringstream s;
    if(!json["trash_size"].is_null())
        s << "Trash size: " << (unsigned int) json["trash_size"].int_value() / (1024*1024) << " Mb\n";
    if(!json["total_space"].is_null())
        s << "Total space: " << (unsigned int) json["total_space"].int_value() / (1024*1024*1024) << " Gb\n";
    if(!json["used_space"].is_null())
        s << "Used space: " << (unsigned int) json["used_space"].int_value() / (1024*1024) << " Mb\n";

    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
    std::u16string utf16 = convert.from_bytes(s.str());

    return wcharstring((WCHAR*)utf16.data());
}

pResources prepare_folder_result(json11::Json json, BOOL isRoot)
{
    if(json["_embedded"]["total"].is_null() || json["_embedded"]["items"].is_null())
        throw std::runtime_error("Wrong Json format");

    int total = json["_embedded"]["total"].int_value();

    pResources pRes = new tResources;
    pRes->nSize = isRoot ? total+1: total;
    pRes->nCount = 0;
    pRes->resource_array = new WIN32_FIND_DATAW[isRoot ? total+1: total];

    std::vector<json11::Json> items = json["_embedded"]["items"].array_items();
    for(int i=0; i<total; i++){
        json11::Json item = items[i];

        std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
        std::u16string utf16 = convert.from_bytes(item["name"].string_value());
        size_t str_size = (MAX_PATH > utf16.size()+1)? (utf16.size()+1): MAX_PATH;
        memcpy(pRes->resource_array[i].cFileName, utf16.data(), sizeof(WCHAR) * str_size);

        if(item["type"].string_value() == "file") {
            pRes->resource_array[i].dwFileAttributes = 0;
            pRes->resource_array[i].nFileSizeLow = (DWORD) item["size"].int_value();
            pRes->resource_array[i].nFileSizeHigh = 0;
        } else {
            pRes->resource_array[i].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
            pRes->resource_array[i].nFileSizeLow = 0;
            pRes->resource_array[i].nFileSizeHigh = 0;
        }
        pRes->resource_array[i].ftCreationTime = parse_iso_time(item["created"].string_value());
        pRes->resource_array[i].ftLastWriteTime = parse_iso_time(item["modified"].string_value());
    }

    if(isRoot){
        memcpy(pRes->resource_array[total].cFileName, u".Trash", sizeof(WCHAR) * 7);
        pRes->resource_array[total].dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
        pRes->resource_array[total].nFileSizeLow = 0;
        pRes->resource_array[total].nFileSizeHigh = 0;
    }

    return pRes;
}

std::string narrow(const std::wstring & wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    return convert.to_bytes(wstr);
}

std::wstring widen(const std::string & str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> convert;
    return convert.from_bytes(str);
}

std::wstring widen(const WCHAR* p)
{
#ifdef _WIN32
    //sizeof(wchar_t) == sizeof(WCHAR) == 2 byte
    return std::wstring((wchar_t*)p);
#else
    //sizeof(wchar_t) > sizeof(WCHAR) in Linux
    int nCount = 0, i=0;
    WCHAR* tmp = (WCHAR*) p;
    while(*tmp++) nCount++;

    std::wstring result(nCount+1, '\0');
    while(*p){
        result[i++] = (wchar_t) *(p++);
    }
    result[i] = '\0';

    return result;
#endif //_WIN32
}

std::vector<wcharstring> split(const wcharstring s, WCHAR separator)
{
    std::vector<std::basic_string<WCHAR> > result;

    int prev_pos = 0, pos = 0;
    while(pos <= s.size()){
        if(s[pos] == separator){
            result.push_back(s.substr(prev_pos, pos-prev_pos));
            prev_pos = ++pos;
        }
        pos++;
    }

    result.push_back(s.substr(prev_pos, pos-prev_pos));

    return result;
}


#endif //YDISK_COMMANDER_PLUGIN_UTILS_H
