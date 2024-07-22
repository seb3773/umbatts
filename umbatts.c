#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>

GtkStatusIcon *global_status_icon;

typedef struct {
    int percentage;
    gboolean charging;
    gboolean full;
} BatteryStatus;

//static void on_quit_menu_item_activate(GtkMenuItem *menu_item, gpointer user_data) {
//    gtk_main_quit();
//}

// static void on_status_icon_popup_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, gpointer user_data) {
//     GtkWidget *menu;
//     GtkWidget *quit_item;
//     menu = gtk_menu_new();
//     quit_item = gtk_menu_item_new_with_label("Quit");
//     g_signal_connect(quit_item, "activate", G_CALLBACK(on_quit_menu_item_activate), NULL);
//     gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);
//     gtk_widget_show_all(menu);
//     gtk_menu_popup(GTK_MENU(menu), NULL, NULL, gtk_status_icon_position_menu, status_icon, button, activate_time);
// }

static char* get_acpi_output() {
    FILE *fp;
    char buffer[256];
    char *output = NULL;
    size_t output_size = 0;
    fp = popen("acpi -b", "r");
    if (fp == NULL) {
        return g_strdup("Failed to run acpi command");
    }
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        char *start = strstr(buffer, ": ");
        if (start != NULL) {
            start += 2; 
            output = g_strdup(start);
        } else {
            output = g_strdup(buffer);
        }
    }
    pclose(fp);
    return output ? output : g_strdup("No battery information available");
}

static BatteryStatus get_battery_status() {
    BatteryStatus status = {0, FALSE, FALSE};
    FILE *fp;
    char buffer[128];
    fp = popen("acpi -b", "r");
    if (fp == NULL) {
        g_print("Failed to run command\n");
        return status;
    }
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        if (strstr(buffer, "Charging") != NULL) {
            status.charging = TRUE;
        } else if (strstr(buffer, "Full") != NULL) {
            status.full = TRUE;
            status.charging = TRUE;
        } else if (strstr(buffer, "Discharging") == NULL) {
            status.charging = TRUE;
        }
        char *percent = strstr(buffer, "%");
        if (percent != NULL) {
            while (*percent != ' ' && percent > buffer) {
                percent--;
            }
            sscanf(percent, "%d", &status.percentage);
        }
    }
    pclose(fp);
    return status;
}

static void update_battery_icon() {
    BatteryStatus status = get_battery_status();
    gchar *icon_name;
    int level = (status.percentage / 10) * 10;
    if (level > 100) level = 100;
    if (level < 0) level = 0;
    if (status.charging) {
        if (status.full || status.percentage == 100) {
            icon_name = g_strdup_printf("/usr/share/icons/kdeten_light/scalable/status/battery-level-100-charged-symbolic.png");
        } else {
            icon_name = g_strdup_printf("/usr/share/icons/kdeten_light/scalable/status/battery-level-%d-charging-symbolic.png", level);
        }
    } else {
        icon_name = g_strdup_printf("/usr/share/icons/kdeten_light/scalable/status/battery-level-%d-symbolic.png", level);
    }
    gtk_status_icon_set_from_file(global_status_icon, icon_name);
    g_free(icon_name);
    char *acpi_output = get_acpi_output();
    gtk_status_icon_set_tooltip_text(global_status_icon, acpi_output);
    g_free(acpi_output);
}

static gboolean update_battery_status(gpointer user_data) {
    update_battery_icon();
    return G_SOURCE_CONTINUE;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    global_status_icon = gtk_status_icon_new();
    gtk_status_icon_set_visible(global_status_icon, TRUE);
//    g_signal_connect(G_OBJECT(global_status_icon), "popup-menu", G_CALLBACK(on_status_icon_popup_menu), NULL);
    update_battery_icon();
    g_timeout_add_seconds(5, update_battery_status, NULL); 
    gtk_main();
    return 0;
}
