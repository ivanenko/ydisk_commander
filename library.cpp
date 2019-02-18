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

#include <iostream>
#include <cstring>
#include <codecvt>
#include <time.h>

#include "library.h"
#include "wfxplugin.h"
#include "restclient.h"
#include "plugin_utils.h"
#include "dialogs.h"


int gPluginNumber, gCryptoNr;
tProgressProcW gProgressProcW = NULL;
tLogProcW gLogProcW = NULL;
tRequestProcW gRequestProcW = NULL;
tCryptProcW gCryptProcW = NULL;

std::string oauth_token, config_file_path;

YdiskRestClient rest_client;


void DCPCALL FsGetDefRootName(char* DefRootName,int maxlen)
{
    strncpy(DefRootName, "Yandex Disk", maxlen);
}

void DCPCALL FsSetCryptCallbackW(tCryptProcW pCryptProcW, int CryptoNr, int Flags)
{
    gCryptoNr = CryptoNr;
    gCryptProcW = pCryptProcW;
}

void DCPCALL FsSetDefaultParams(FsDefaultParamStruct* dps)
{
    std::string defaultIni(dps->DefaultIniName);
    int p = defaultIni.find_last_of('/');
    config_file_path = defaultIni.substr(0, p+1) + "yandex_disk.ini";
}

int DCPCALL FsInitW(int PluginNr, tProgressProcW pProgressProc, tLogProcW pLogProc, tRequestProcW pRequestProc)
{
    gProgressProcW = pProgressProc;
    gLogProcW = pLogProc;
    gRequestProcW = pRequestProc;
    gPluginNumber = PluginNr;

    return 0;
}

HANDLE DCPCALL FsFindFirstW(WCHAR* Path, WIN32_FIND_DATAW *FindData)
{
    memset(FindData, 0, sizeof(WIN32_FIND_DATAW));

    wcharstring wPath(Path);
    std::replace(wPath.begin(), wPath.end(), u'\\', u'/');

    if(oauth_token.empty())
        oauth_token = get_oauth_token(config_file_path, gPluginNumber, gRequestProcW, gCryptoNr, gCryptProcW);

    if(oauth_token.empty())
        return (HANDLE)-1;

    rest_client.set_oauth_token(oauth_token.c_str());

    pResources pRes = NULL;
    try{
         int ifTrash = wPath.find((WCHAR*)u"/.Trash");
         if(ifTrash == 0)
             wPath = wPath.substr(7);

         json11::Json result = rest_client.get_resources(toUTF8(wPath.data()), ifTrash == 0);
         pRes = prepare_folder_result(result, wPath == (WCHAR*)u"/");
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
    }

    if(!pRes || pRes->nSize==0)
        return (HANDLE)-1;

    if(pRes->resource_array && pRes->nSize>0)
        memcpy(FindData, pRes->resource_array, sizeof(WIN32_FIND_DATAW));

    return (HANDLE) pRes;
}


BOOL DCPCALL FsFindNextW(HANDLE Hdl,WIN32_FIND_DATAW *FindData)
{
    pResources pRes = (pResources) Hdl;
    pRes->nCount++;
    if(pRes->nCount < pRes->nSize){
        memcpy(FindData, &pRes->resource_array[pRes->nCount], sizeof(WIN32_FIND_DATAW));
        return true;
    } else {
        return false;
    }
}

int DCPCALL FsFindClose(HANDLE Hdl){
    pResources pRes = (pResources) Hdl;
    if(pRes){
        if(pRes->resource_array)
            delete[] pRes->resource_array;

        delete pRes;
    }

    return 0;
}

BOOL DCPCALL FsMkDir(char* Path){
    int i;
    return false;
}

BOOL DCPCALL FsMkDirW(WCHAR* Path)
{
    try{
        wcharstring wstrPath(Path);
        std::replace(wstrPath.begin(), wstrPath.end(), u'\\', u'/');
        rest_client.makeFolder(toUTF8(wstrPath.data()));

        return true;
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
        return false;
    }
}

BOOL DCPCALL FsDeleteFileW(WCHAR* RemoteName)
{
    try{
        wcharstring wPath(RemoteName);
        std::replace(wPath.begin(), wPath.end(), u'\\', u'/');

        if(wPath.find((WCHAR*)u"/.Trash") == 0)
            rest_client.deleteFromTrash(toUTF8(wPath.substr(7).data()));
        else
            rest_client.removeResource(toUTF8(wPath.data()));

        return true;
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
        return false;
    }
}

BOOL DCPCALL FsRemoveDirW(WCHAR* RemoteName)
{
    try{
        wcharstring wstrPath(RemoteName);
        std::replace(wstrPath.begin(), wstrPath.end(), u'\\', u'/');
        rest_client.removeResource(toUTF8(wstrPath.data()));
        return true;
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
        return false;
    }
}

int DCPCALL FsGetFileW(WCHAR* RemoteName, WCHAR* LocalName, int CopyFlags, RemoteInfoStruct* ri)
{
    if(CopyFlags & FS_COPYFLAGS_RESUME)
        return FS_FILE_NOTSUPPORTED;

    try{
        std::ofstream ofs;
        wcharstring wRemoteName(RemoteName), wLocalName(LocalName);
        std::replace(wRemoteName.begin(), wRemoteName.end(), u'\\', u'/');

        BOOL isFileExists = file_exists(toUTF8(wLocalName.data()));
        if(isFileExists && !(CopyFlags & FS_COPYFLAGS_OVERWRITE) )
            return FS_FILE_EXISTS;

        ofs.open(toUTF8(wLocalName.data()), std::ios::binary | std::ofstream::out | std::ios::trunc);
        if(!ofs || ofs.bad())
            return FS_FILE_WRITEERROR;

        int err = gProgressProcW(gPluginNumber, RemoteName, LocalName, 0);
        if(err)
            return FS_FILE_USERABORT;

        rest_client.downloadFile(toUTF8(wRemoteName.data()), ofs);
        gProgressProcW(gPluginNumber, RemoteName, LocalName, 100);

        if(CopyFlags & FS_COPYFLAGS_MOVE)
            FsDeleteFileW(RemoteName);

    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
        return FS_FILE_READERROR;
    }

    //ofstream closes file in destructor
    return FS_FILE_OK;
}

int DCPCALL FsPutFileW(WCHAR* LocalName,WCHAR* RemoteName,int CopyFlags) {
    if (CopyFlags & FS_COPYFLAGS_RESUME)
        return FS_FILE_NOTSUPPORTED;

    try {
        std::ifstream ifs;
        wcharstring wRemoteName(RemoteName), wLocalName(LocalName);
        std::replace(wRemoteName.begin(), wRemoteName.end(), u'\\', u'/');
        ifs.open(toUTF8(wLocalName.data()), std::ios::binary | std::ofstream::in);
        if (!ifs || ifs.bad())
            return FS_FILE_READERROR;

        int err = gProgressProcW(gPluginNumber, LocalName, RemoteName, 0);
        if (err)
            return FS_FILE_USERABORT;
        rest_client.uploadFile(toUTF8(wRemoteName.data()), ifs, (CopyFlags & FS_COPYFLAGS_OVERWRITE));
        gProgressProcW(gPluginNumber, LocalName, RemoteName, 100);

        ifs.close();

        if(CopyFlags & FS_COPYFLAGS_MOVE)
            std::remove(toUTF8(LocalName).c_str());

    } catch(rest_client_exception & e){
        if(e.get_status() == 409){
            return FS_FILE_EXISTS;
        } else {
            gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
            return FS_FILE_WRITEERROR;
        }
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
        return FS_FILE_WRITEERROR;
    }

    return FS_FILE_OK;
}

int DCPCALL FsRenMovFileW(WCHAR* OldName, WCHAR* NewName, BOOL Move, BOOL OverWrite, RemoteInfoStruct* ri)
{
    try{
        wcharstring wOldName(OldName), wNewName(NewName);
        std::replace(wOldName.begin(), wOldName.end(), u'\\', u'/');
        std::replace(wNewName.begin(), wNewName.end(), u'\\', u'/');

        int err = gProgressProcW(gPluginNumber, OldName, NewName, 0);
        if (err)
            return FS_FILE_USERABORT;

        if(Move){
            rest_client.move(toUTF8(wOldName.c_str()), toUTF8(wNewName.c_str()), OverWrite);
        } else {
            rest_client.copy(toUTF8(wOldName.c_str()), toUTF8(wNewName.c_str()), OverWrite);
        }
        gProgressProcW(gPluginNumber, OldName, NewName, 100);
    } catch(rest_client_exception & e){
        if(e.get_status() == 409){
            return FS_FILE_EXISTS;
        } else {
            gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
            return FS_FILE_WRITEERROR;
        }
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
        return FS_FILE_WRITEERROR;
    }

    return FS_FILE_OK;
}

int _download_file(wcharstring remoteName, wcharstring url, wcharstring filename)
{
    try{
        wcharstring fname;
        if(filename.empty()) {
            // extract filename from url
            std::string::size_type p = url.find_last_of((WCHAR)u'/');
            if(p == std::string::npos)
                return FS_EXEC_ERROR;

            fname = remoteName + url.substr(p+1);
        } else {
            fname = remoteName += filename;
        }

        rest_client.saveFromUrl(toUTF8(url.c_str()), toUTF8(fname.c_str()));
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
        return FS_FILE_WRITEERROR;
    }

    return FS_EXEC_OK;
}

int DCPCALL FsExecuteFileW(HWND MainWin, WCHAR* RemoteName, WCHAR* Verb)
{
    try{
        wcharstring wVerb(Verb), wRemoteName(RemoteName);

        if(wVerb.find((WCHAR*)u"quote") == 0){
            std::vector<wcharstring> strings = split(wVerb, (WCHAR)u' ');
            if(strings.size()<2)
                return FS_EXEC_ERROR;

            if(strings[1] == (WCHAR*)u"download"){
                if(strings.size()==3)
                    return _download_file(wRemoteName, strings[2], wcharstring());
                else if(strings.size()==4)
                    return _download_file(wRemoteName, strings[2], strings[3]);
                else
                    return FS_EXEC_ERROR;
            } else if(strings[1] == (WCHAR*)u"trash"){
                if(strings.size()==3 && strings[2]==(WCHAR*)u"clean"){
                    int res = gRequestProcW(gPluginNumber, RT_MsgOKCancel, (WCHAR*)u"Warning", (WCHAR*)u"All files from trash will be removed!", NULL, 0);
                    if(res != 0){
                        try {
                            rest_client.cleanTrash();
                        } catch (std::runtime_error & e){
                            gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
                        }
                    }
                }

                if(strings.size()==2){
                    WCHAR* p = (WCHAR*)u"/trash";
                    memcpy(RemoteName, p, sizeof(WCHAR) * 7);
                    return FS_EXEC_SYMLINK;
                }
            } else {
                gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error", (WCHAR*)u"Not supported command", NULL, 0);
                return FS_EXEC_ERROR;
            }
        }

        if(wVerb.find((WCHAR*)u"properties") == 0){
            if(wRemoteName == (WCHAR*) u"/"){ // show disk properties
                //wcharstring disk_info = prepare_disk_info(rest_client.get_disk_info());
                //gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Disk properties", (WCHAR*) disk_info.data(), NULL, 0);
                show_plugin_properties_dlg(MainWin, config_file_path);
            }
        }
    } catch (std::runtime_error & e){
        gRequestProcW(gPluginNumber, RT_MsgOK, (WCHAR*)u"Error command", (WCHAR*) fromUTF8(e.what()).c_str(), NULL, 0);
        return FS_EXEC_ERROR;
    }

    return FS_EXEC_OK;

}




