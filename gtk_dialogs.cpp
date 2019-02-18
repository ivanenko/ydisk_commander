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

#include <gtk/gtk.h>
#include <fstream>
#include <sstream>
#include "dialogs.h"
#include "json11.hpp"

void show_plugin_properties_dlg(void *hMainWnd, std::string config_path)
{
    json11::Json json;
    std::ifstream ifs;
    ifs.open(config_path, std::ios::binary | std::ofstream::in);
    if (ifs && !ifs.bad()){
        std::stringstream body;
        body << ifs.rdbuf();
        ifs.close();

        std::string error;
        json = json11::Json::parse(body.str(), error);
    }

    GtkWidget *dialog, *button1, *button2;

    dialog = gtk_dialog_new_with_buttons("Yandex Disk account settings", GTK_WINDOW(hMainWnd),
                                         GTK_DIALOG_MODAL,
                                         GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                                         NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 250);

    g_signal_connect (G_OBJECT (dialog), "destroy", gtk_main_quit, NULL);

    /* Create Frame 1 */
    GtkWidget *frame = gtk_frame_new ("OAuth token");
    gtk_box_pack_start(GTK_BOX (GTK_DIALOG(dialog)->vbox), frame, FALSE, FALSE, 10);
    GtkWidget *v_box = gtk_vbox_new (TRUE, 0);
    gtk_container_add (GTK_CONTAINER (frame), v_box);
    gtk_widget_show (v_box);

    // radiobuttons
    button1 = gtk_radio_button_new_with_label (NULL, "Get OAuth token from Yandex");
    //gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox), button1, FALSE, FALSE, 5);
    gtk_box_pack_start (GTK_BOX (v_box), button1, FALSE, FALSE, 2);
    gtk_widget_show (button1);

    GSList *group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button1));
    button2 = gtk_radio_button_new_with_label (group, "Enter token manually");
    gtk_box_pack_start (GTK_BOX (v_box), button2, FALSE, FALSE, 2);
    if(json["get_token_method"].is_string() && json["get_token_method"].string_value() == "manual")
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button2), TRUE);
    gtk_widget_show (button2);

    gtk_widget_show (frame);


    /* Create Frame 2 */
    frame = gtk_frame_new ("Save method");
    gtk_box_pack_start(GTK_BOX (GTK_DIALOG(dialog)->vbox), frame, FALSE, FALSE, 10);
    v_box = gtk_vbox_new (TRUE, 0);
    gtk_container_add (GTK_CONTAINER (frame), v_box);
    gtk_widget_show (v_box);

    //radiobutton2
    GtkWidget *btn1, *btn2, *btn3;

    btn1 = gtk_radio_button_new_with_label (NULL, "Do not save token");
    gtk_box_pack_start (GTK_BOX (v_box), btn1, FALSE, FALSE, 2);
    gtk_widget_show (btn1);

    btn2 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(btn1), "Save token in config file");
    gtk_box_pack_start (GTK_BOX (v_box), btn2, FALSE, FALSE, 2);
    if(json["save_type"].is_string() && json["save_type"].string_value() == "config")
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn2), TRUE);
    gtk_widget_show (btn2);

    btn3 = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON(btn1), "Save token in Totalcmd password manager");
    gtk_box_pack_start (GTK_BOX (v_box), btn3, FALSE, FALSE, 2);
    if(json["save_type"].is_string() && json["save_type"].string_value() == "password_manager")
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn3), TRUE);
    gtk_widget_show (btn3);

    gtk_widget_show (frame);


//    GtkWidget *separator = gtk_hseparator_new ();
//    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox), separator, FALSE, TRUE, 0);
//    gtk_widget_show (separator);


    gint result = gtk_dialog_run (GTK_DIALOG (dialog));

    //json11::Json json;
    json11::Json::object json_obj = json.object_items();
    std::ofstream ofs;
    switch (result)
    {
        case GTK_RESPONSE_ACCEPT:

            if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(button1) ))
                json_obj["get_token_method"] = "oauth";
            if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(button2) )){
                json_obj["get_token_method"] = "manual";
            }

            if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(btn1) )){
                json_obj["save_type"] = "dont_save";
                json_obj["oauth_token"] = "";
            }
            if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(btn2) ))
                json_obj["save_type"] = "config";
            if (gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(btn3) )){
                json_obj["save_type"] = "password_manager";
                json_obj["oauth_token"] = "";
            }

            ofs.open(config_path, std::ofstream::out | std::ios::trunc);
            ofs << json11::Json(json_obj).dump();
            ofs.close();

            break;
        default:
            //
            break;
    }
    gtk_widget_destroy(dialog);

}

