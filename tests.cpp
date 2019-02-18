//
// Created by danil on 1/31/19.
//
#include <cstring>
#include <codecvt>
#include <locale>
#include <string>
#include <iostream>
#include <sstream>
#include <time.h>
#include "wfxplugin.h"
#include "json11.hpp"

class Wrapper
{
private:
    WCHAR* _data;

public:

    Wrapper(const char* input)
    {
        int length = strlen(input) + 1;
        _data = new WCHAR[length];
        WCHAR* ptr = _data;
        while(*input){
            *(ptr++) = (WCHAR) *(input++);
        }
        *ptr = '\0';
    }

    ~Wrapper()
    {
        delete[] _data;
        std::cout << "deleted";
    }

    operator WCHAR*()
    {
        return _data;
    }
};

typedef std::basic_string<WCHAR> tt;

void test_func(WCHAR* dd){
    std::cout << dd << std::endl;

    throw std::runtime_error("the error");
}

//-----------------------------------------------

class rest_client_exception: public std::runtime_error
{
public:
    rest_client_exception(int status_code, const std::string& message):
            status_code(status_code),
            std::runtime_error(message)
    {
        std::stringstream s;
        s << "Error code: " << std::to_string(status_code) << "; " << std::runtime_error::what();
        msg_ = s.str();
    }

    virtual const char* what() const throw () {
//        std::stringstream s;
//        s << "Error code: " << std::to_string(status_code) << "; " << std::runtime_error::what();
//        msg_ = s.str();
        return msg_.c_str();
    }

    int get_status(){ return status_code; }

    std::string get_message(){ return std::runtime_error::what(); }

protected:
    int status_code;
    std::string msg_;
};

void mm(){
    throw rest_client_exception(2000, "the error!!");
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

int main(void){

    const char16_t* zz = u"sds";

    std::cout << zz << std::endl;


    try {
        test_func((WCHAR *) u"123123");
    } catch (std::runtime_error & e){
        std::cout << e.what() << std::endl;
    }

    WCHAR* p1 = (WCHAR*)u"123qwe";

    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
    std::u16string utf16 = convert.from_bytes(std::string(u8"z\u00df\u6c34\u6c34"));

    std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert2;
    std::string sss = convert2.to_bytes(std::u16string((char16_t*) p1));

    //widen wchar to wchar_t
    std::wstring ws(10, '\0');

    //test_func((WCHAR*)utf16.data());

    //tt ee =  std::wstring_convert<
    //        std::codecvt_utf8_utf16<WCHAR>, WCHAR>{}
    //        .from_bytes(std::string(u8"z\u00df\u6c34\u6c34"));
    //std::wstring s2 = widen(item["name"].string_value());
    //wchart_to_wchar(pRes->resource_array[i].cFileName, s2.c_str(), MAX_PATH);
    //memcpy(pRes->resource_array[i].cFileName, ee.data(), sizeof(WCHAR) * ee.size());


    //Wrapper w("zzzzzz");
    //test_func(Wrapper("ololo"));
    std::cout << "\nend of programm\n--------------\n";
    auto d = sizeof(WCHAR);
    auto d2 = sizeof(wchar_t);

//    std::string err;
//    json11::Json j = json11::Json::parse("{\"q\": 11}", err);
//    auto d = j["q"];
//    auto f = j["zz"]["xx"];
//    auto bb = f.is_null();
//
//    try{
//        mm();
//    } catch(std::runtime_error& e){
//        std::cout << e.what() << '\n';
//    }

//    struct tm t, tz;
//    strptime("2014-04-22T10:32:49-04:00", "%Y-%m-%dT%H:%M:%S%z", &t);

    //time_t t2 = mktime(&t);

//    std::string st("2014-04-22T10:32:49+04:00");
//    std::string zz1 = st.substr(st.size()-6);
//    strptime(zz1.c_str(), "+%H:%M", &tz);

//    std::wstring s22 = widen((WCHAR*)u"123");

    std::basic_string<WCHAR> sw = (WCHAR*)u"http://ololo.ru/bla/bla/photo.jpg";
    int p = sw.find_last_of((WCHAR)u'/');
    std::basic_string<WCHAR> o = sw.substr(0, p+1);

    json11::Json my_json = json11::Json::object { {"key1", "value1"}, {"key2", "value2"} };
    json11::Json::object json_obj = my_json.object_items();
    json_obj["key1"] = "ololo";
    std::string ddd = json11::Json(json_obj).dump();

    //gFix=gtk_vbox_new(true,5);
    //gtk_container_add(GTK_CONTAINER(GtkWidget(MainWin)),gFix);
    //gtk_widget_show(gFix);

    std::basic_string<WCHAR> sw1 = (WCHAR*)u"/.Trash/sss";
    std::basic_string<WCHAR> o1 = sw1.substr(7);


    return 0;
}

/*
 * std::string utf16_to_utf8(std::u16string utf16_string)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.to_bytes(utf16_string);
}

 * */