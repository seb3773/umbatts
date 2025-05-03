#include <gtk/gtk.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#define PATH_SIZE 128
#define ICON_PATH_SIZE 256

typedef struct {
    uint8_t percentage;
    uint8_t charging;
} BatteryStatus;

typedef struct {
    char directory[PATH_SIZE];
    int max_brightness;
    int current_brightness;
    int fd_brightness;
} BacklightInfo;

typedef struct {
    char name[16];
    int fd_capacity;
    int fd_status;
} BatteryInfo;


static GtkStatusIcon *status_icon = NULL;
static BatteryInfo battery_info = {{0}, -1, -1};
static BacklightInfo backlight_info = {{0}, 0, 0, -1};
static gboolean warn_batt = FALSE;



static inline gboolean initialize_backlight(void) {
    const char *base_path = "/sys/class/backlight/";
    char path[PATH_SIZE];
    int fd;
    GDir *dir = g_dir_open(base_path, 0, NULL);
    if (!dir) return FALSE;
    const char *entry = g_dir_read_name(dir);
    if (!entry) {
        g_dir_close(dir);
        return FALSE;
    }
    snprintf(backlight_info.directory, PATH_SIZE, "%s%s", base_path, entry);
    g_dir_close(dir);
    snprintf(path, PATH_SIZE, "%s/max_brightness", backlight_info.directory);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        backlight_info.directory[0] = '\0';
        return FALSE;
    }
    char buffer[16];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        backlight_info.max_brightness = atoi(buffer);
    }
    close(fd);
    snprintf(path, PATH_SIZE, "%s/brightness", backlight_info.directory);
    backlight_info.fd_brightness = open(path, O_RDWR);
    if (backlight_info.fd_brightness < 0) {
        backlight_info.directory[0] = '\0';
        return FALSE;
    }
    n = read(backlight_info.fd_brightness, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        backlight_info.current_brightness = atoi(buffer);
    }
    return TRUE;
}



static inline void find_battery_name(void) {
    if (battery_info.name[0]) return;
    const char *base_path = "/sys/class/power_supply/";
    char path[PATH_SIZE];
    FILE *fp;
    GDir *dir = g_dir_open(base_path, 0, NULL);
    if (!dir) return;
    const char *entry;
    while ((entry = g_dir_read_name(dir))) {
        snprintf(path, PATH_SIZE, "%s%s/type", base_path, entry);
        fp = fopen(path, "r");
        if (fp) {
            char type[16];
            if (fgets(type, sizeof(type), fp) && strstr(type, "Battery")) {
                strncpy(battery_info.name, entry, sizeof(battery_info.name) - 1);
                fclose(fp);
                break;
            }
            fclose(fp);
        }
    }
    g_dir_close(dir);
    if (battery_info.name[0]) {
        snprintf(path, PATH_SIZE, "%s%s/capacity", base_path, battery_info.name);
        battery_info.fd_capacity = open(path, O_RDONLY);
        snprintf(path, PATH_SIZE, "%s%s/status", base_path, battery_info.name);
        battery_info.fd_status = open(path, O_RDONLY);
    }
}




static inline BatteryStatus get_battery_status(void) {
    BatteryStatus status = {0, 0};
    if (!battery_info.name[0] || battery_info.fd_capacity < 0 || battery_info.fd_status < 0) {
        return status;
    }
    char buffer[16];
    ssize_t n;
    lseek(battery_info.fd_capacity, 0, SEEK_SET);
    n = read(battery_info.fd_capacity, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        int percentage = atoi(buffer);
        if (percentage >= 0 && percentage <= 100) {
            status.percentage = (uint8_t)percentage;
        }
    }
    lseek(battery_info.fd_status, 0, SEEK_SET);
    n = read(battery_info.fd_status, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        if (strstr(buffer, "Charging")) status.charging = 1;
        else if (strstr(buffer, "Full")) status.charging = 2;
    }
    return status;
}

static inline char *get_battery_info(void) {
    BatteryStatus status = get_battery_status();
    static char buffer[32];
    const char *state = status.charging == 2 ? "Full" : 
                        status.charging == 1 ? "Charging" : "Discharging";
    snprintf(buffer, sizeof(buffer), "Battery: %d%%, %s", status.percentage, state);
    return buffer;
}

static void show_low_battery_warning(void) {
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
        "Low Battery Warning\nBattery level is critically low. Please connect to a power source.");
    gtk_window_set_title(GTK_WINDOW(dialog), "Battery Warning");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    warn_batt = TRUE;
}

static void slider_changed(GtkRange *range, gpointer user_data) {
    if (!backlight_info.directory[0] || backlight_info.fd_brightness < 0) return;
    double value = gtk_range_get_value(range);
    int brightness = (int)((value / 100.0) * backlight_info.max_brightness);
    if (brightness < 1) brightness = 1;
    char buffer[16];
    int len = snprintf(buffer, sizeof(buffer), "%d\n", brightness);
    lseek(backlight_info.fd_brightness, 0, SEEK_SET);
    write(backlight_info.fd_brightness, buffer, len);
}

static gboolean hide_popup_timeout(gpointer window) {
    if (window && gtk_widget_get_visible(GTK_WIDGET(window))) {
        gtk_widget_destroy(GTK_WIDGET(window));
    }
    return G_SOURCE_REMOVE;
}



static void show_slider_popup(GtkStatusIcon *icon, guint button, guint activate_time, gpointer user_data) {
    GtkWidget *popup_window = gtk_window_new(GTK_WINDOW_POPUP);
    GtkWidget *vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(popup_window), vbox);
    GtkWidget *acpi_label = gtk_label_new(get_battery_info());
    gtk_box_pack_start(GTK_BOX(vbox), acpi_label, FALSE, FALSE, 2);
    if (backlight_info.directory[0] && backlight_info.fd_brightness >= 0) {
        GtkWidget *slider = gtk_hscale_new_with_range(0, 100, 1);
        gtk_widget_set_size_request(slider, 200, -1);
        g_signal_connect(slider, "value-changed", G_CALLBACK(slider_changed), NULL);
        if (backlight_info.max_brightness > 0) {
            double slider_value = (backlight_info.current_brightness * 100.0) / backlight_info.max_brightness;
            gtk_range_set_value(GTK_RANGE(slider), slider_value);
        }
        gtk_box_pack_start(GTK_BOX(vbox), slider, TRUE, TRUE, 2);
    }
    const char *info = gtk_label_get_text(GTK_LABEL(acpi_label));
    int popup_width = strlen(info) * 8 + 20;
    if (popup_width < 320) popup_width = 320;
    gtk_widget_set_size_request(popup_window, popup_width, -1);
    gtk_widget_show_all(popup_window);
    GdkScreen *screen = gdk_screen_get_default();
    int screen_width = gdk_screen_get_width(screen);
    int screen_height = gdk_screen_get_height(screen);
    int y = backlight_info.directory[0] ? screen_height - 110 : screen_height - 60;
    gtk_window_move(GTK_WINDOW(popup_window), screen_width - popup_width - 10, y);
    g_timeout_add_seconds(3, hide_popup_timeout, popup_window);
}

static inline void update_battery_icon(void) {
    BatteryStatus status = get_battery_status();
    char icon_path[ICON_PATH_SIZE];
    int level = (status.percentage / 10) * 10;
    if (level > 100) level = 100;
    if (level < 0) level = 0;
    if (status.percentage <= 15 && !status.charging && !warn_batt) {
        show_low_battery_warning();
    }
    if (status.percentage > 16 || status.charging) {
        warn_batt = FALSE;
    }
    const char *suffix = status.charging ? "-charging" : "";
    const char *icon_base = "/usr/share/icons/kdeten_light/scalable/status/battery-level-%d%s-symbolic.png";
    if (status.charging == 2 || status.percentage == 100) {
        snprintf(icon_path, ICON_PATH_SIZE, 
                 "/usr/share/icons/kdeten_light/scalable/status/battery-level-100-charged-symbolic.png");
    } else {
        snprintf(icon_path, ICON_PATH_SIZE, icon_base, level, suffix);
    }
    gtk_status_icon_set_from_file(status_icon, icon_path);
    gtk_status_icon_set_tooltip_text(status_icon, get_battery_info());
}


static gboolean update_battery_status(gpointer user_data) {
    update_battery_icon();
    return G_SOURCE_CONTINUE;
}

static void cleanup(void) {
    if (battery_info.fd_capacity >= 0) close(battery_info.fd_capacity);
    if (battery_info.fd_status >= 0) close(battery_info.fd_status);
    if (backlight_info.fd_brightness >= 0) close(backlight_info.fd_brightness);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    initialize_backlight();
    find_battery_name();
    status_icon = gtk_status_icon_new();
    gtk_status_icon_set_visible(status_icon, TRUE);
    g_signal_connect(status_icon, "popup-menu", G_CALLBACK(show_slider_popup), NULL);
    g_signal_connect(status_icon, "activate", G_CALLBACK(show_slider_popup), NULL);
    update_battery_icon();
    g_timeout_add_seconds(5, update_battery_status, NULL);
    g_atexit(cleanup);
    gtk_main();
    return 0;
}