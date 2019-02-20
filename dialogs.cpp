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

#include <fstream>
#include <sstream>
#include <cstring>
#include "extension.h"
#include "json11.hpp"
#include "dialogs.h"

const char* config_dialog = R"(
object DialogBox: TDialogBox
  Left = 2287
  Height = 251
  Top = 280
  Width = 426
  AutoSize = True
  BorderIcons = [biSystemMenu]
  BorderStyle = bsDialog
  Caption = 'Yandex Disk account settings'
  ChildSizing.LeftRightSpacing = 10
  ChildSizing.TopBottomSpacing = 10
  ClientHeight = 251
  ClientWidth = 426
  OnShow = DialogBoxShow
  Position = poScreenCenter
  LCLVersion = '1.8.4.0'

  object btnOK: TBitBtn
    Left = 256
    Height = 30
    Top = 208
    Width = 75
    Default = True
    DefaultCaption = True
    Kind = bkOK
    ModalResult = 1
    OnClick = ButtonClick
    TabOrder = 2
  end
  object btnCancel: TBitBtn
    Left = 341
    Height = 30
    Top = 208
    Width = 75
    Cancel = True
    DefaultCaption = True
    Kind = bkCancel
    ModalResult = 2
    OnClick = ButtonClick
    TabOrder = 3
  end

  object getTokenRadio: TRadioGroup
    Left = 8
    Height = 72
    Top = 8
    Width = 408
    AutoFill = True
    Caption = 'OAuth token'
    ChildSizing.LeftRightSpacing = 6
    ChildSizing.EnlargeHorizontal = crsHomogenousChildResize
    ChildSizing.EnlargeVertical = crsHomogenousChildResize
    ChildSizing.ShrinkHorizontal = crsScaleChilds
    ChildSizing.ShrinkVertical = crsScaleChilds
    ChildSizing.Layout = cclLeftToRightThenTopToBottom
    ChildSizing.ControlsPerLine = 1
    ClientHeight = 53
    ClientWidth = 404
    Items.Strings = (
      'Get OAuth token from Yandex'
      'Enter token manually'
    )
    TabOrder = 0
  end
  object saveMethodRadio: TRadioGroup
    Left = 8
    Height = 105
    Top = 88
    Width = 409
    AutoFill = True
    Caption = 'Save method'
    ChildSizing.LeftRightSpacing = 6
    ChildSizing.EnlargeHorizontal = crsHomogenousChildResize
    ChildSizing.EnlargeVertical = crsHomogenousChildResize
    ChildSizing.ShrinkHorizontal = crsScaleChilds
    ChildSizing.ShrinkVertical = crsScaleChilds
    ChildSizing.Layout = cclLeftToRightThenTopToBottom
    ChildSizing.ControlsPerLine = 1
    ClientHeight = 86
    ClientWidth = 405
    Items.Strings = (
      'Do not save token'
      'Save token in config file'
      'Save token in Totalcmd password manager'
    )
    TabOrder = 1
  end
end
)";

tExtensionStartupInfo *gExtension = NULL;
std::string gConfig_path;
json11::Json gJson;

intptr_t DCPCALL DlgProc(uintptr_t pDlg, char *DlgItemName, intptr_t Msg, intptr_t wParam, intptr_t lParam)
{
    switch (Msg){
        case DN_INITDIALOG:
            if(gJson["get_token_method"].is_string() && gJson["get_token_method"].string_value() == "manual")
                gExtension->SendDlgMsg(pDlg, "getTokenRadio", DM_LISTSETITEMINDEX, 1, 0);
            else
                gExtension->SendDlgMsg(pDlg, "getTokenRadio", DM_LISTSETITEMINDEX, 0, 0);

            if(gJson["save_type"].is_string() && gJson["save_type"].string_value() == "config")
                gExtension->SendDlgMsg(pDlg, "saveMethodRadio", DM_LISTSETITEMINDEX, 1, 0);
            else if(gJson["save_type"].is_string() && gJson["save_type"].string_value() == "password_manager")
                gExtension->SendDlgMsg(pDlg, "saveMethodRadio", DM_LISTSETITEMINDEX, 2, 0);
            else
                gExtension->SendDlgMsg(pDlg, "saveMethodRadio", DM_LISTSETITEMINDEX, 0, 0); // dont_save by default

            break;

        case DN_CHANGE:
            break;

        case DN_CLICK:
            if(strcmp(DlgItemName, "btnOK")==0){
                json11::Json::object json_obj = gJson.object_items();
                std::ofstream ofs;

                int res = gExtension->SendDlgMsg(pDlg, "getTokenRadio", DM_LISTGETITEMINDEX, 0, 0);
                json_obj["get_token_method"] = (res==0)? "oauth" : "manual";

                res = gExtension->SendDlgMsg(pDlg, "saveMethodRadio", DM_LISTGETITEMINDEX, 0, 0);
                switch(res){
                    case 1:
                        json_obj["save_type"] = "config";
                        break;
                    case 2:
                        json_obj["save_type"] = "password_manager";
                        json_obj["oauth_token"] = "";
                        break;
                    default:
                        json_obj["save_type"] = "dont_save";
                        json_obj["oauth_token"] = "";
                }

                ofs.open(gConfig_path, std::ofstream::out | std::ios::trunc);
                ofs << json11::Json(json_obj).dump();
                ofs.close();

                gExtension->SendDlgMsg(pDlg, DlgItemName, DM_CLOSE, 1, 0);
            } else if(strcmp(DlgItemName, "btnCancel")==0){
                gExtension->SendDlgMsg(pDlg, DlgItemName, DM_CLOSE, 1, 0);
            }

            break;
    }
    return 0;
}

void show_plugin_properties_dlg(void *hMainWnd, std::string config_path, tExtensionStartupInfo *pExtension)
{
    gExtension = pExtension;
    gConfig_path = config_path;

    std::ifstream ifs;
    ifs.open(config_path, std::ios::binary | std::ofstream::in);
    if (ifs && !ifs.bad()) {
        std::stringstream body;
        body << ifs.rdbuf();
        ifs.close();

        std::string error;
        gJson = json11::Json::parse(body.str(), error);
    }

    gExtension->DialogBoxLFM((intptr_t)config_dialog, strlen(config_dialog), DlgProc);
}


