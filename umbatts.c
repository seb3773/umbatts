#include <gtk/gtk.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include "battery_icons.h"

#define PATH_SIZE 128
#define TOOLTIP_BUFFER_SIZE 32

#define FAST_PARSE_INT(buf, n) ({ \
    int v = 0; \
    for (ssize_t i = 0; i < (n); ++i) { \
        if ((buf)[i] < '0' || (buf)[i] > '9') break; \
        v = v * 10 + ((buf)[i] - '0'); \
    } \
    v; \
})

#define READ_CURRENT_BRIGHTNESS() ({ \
    gboolean _ret = FALSE; \
    if (backlight_fd_brightness >= 0) { \
        char _buffer[8]; \
        ssize_t _n = pread(backlight_fd_brightness, _buffer, sizeof(_buffer) - 1, 0); \
        if (_n > 0) { \
            int _v = 0; \
            for (ssize_t _i = 0; _i < _n; ++_i) { \
                if (_buffer[_i] < '0' || _buffer[_i] > '9') break; \
                _v = _v * 10 + (_buffer[_i] - '0'); \
            } \
            backlight_current_brightness = _v; \
            _ret = TRUE; \
        } \
    } \
    _ret; \
})



#define GET_BATTERY_STATUS() \
    do { \
        char b[8]; \
        ssize_t n = pread(battery_fd_capacity, b, sizeof(b), 0); \
        battery_percentage = (n > 0) ? (FAST_PARSE_INT(b, n) > 100 ? 100 : FAST_PARSE_INT(b, n)) : 0; \
        n = pread(battery_fd_status, b, sizeof(b) - 1, 0); \
        battery_charging = (n > 0) ? (b[0] == 'C' ? 1 : (b[0] == 'F' ? 2 : 0)) : 0; \
    } while(0)


static gboolean slider_active = FALSE;

static const char *embedded_icon_names[] = {
    "battery-level-0-symbolic", "battery-level-10-symbolic", "battery-level-20-symbolic", "battery-level-30-symbolic",
    "battery-level-40-symbolic", "battery-level-50-symbolic", "battery-level-60-symbolic", "battery-level-70-symbolic",
    "battery-level-80-symbolic", "battery-level-90-symbolic", "battery-level-100-symbolic",
    "battery-level-0-charging-symbolic", "battery-level-10-charging-symbolic", "battery-level-20-charging-symbolic",
    "battery-level-30-charging-symbolic", "battery-level-40-charging-symbolic", "battery-level-50-charging-symbolic",
    "battery-level-60-charging-symbolic", "battery-level-70-charging-symbolic", "battery-level-80-charging-symbolic",
    "battery-level-90-charging-symbolic", "battery-level-100-charged-symbolic", NULL
};

static const unsigned char *embedded_icon_data[] = {
    battery_level_0_symbolic_data, battery_level_10_symbolic_data, battery_level_20_symbolic_data,
    battery_level_30_symbolic_data, battery_level_40_symbolic_data, battery_level_50_symbolic_data,
    battery_level_60_symbolic_data, battery_level_70_symbolic_data, battery_level_80_symbolic_data,
    battery_level_90_symbolic_data, battery_level_100_symbolic_data, battery_level_0_charging_symbolic_data,
    battery_level_10_charging_symbolic_data, battery_level_20_charging_symbolic_data, battery_level_30_charging_symbolic_data,
    battery_level_40_charging_symbolic_data, battery_level_50_charging_symbolic_data, battery_level_60_charging_symbolic_data,
    battery_level_70_charging_symbolic_data, battery_level_80_charging_symbolic_data, battery_level_90_charging_symbolic_data,
    battery_level_100_charged_symbolic_data, NULL
};

static size_t embedded_icon_sizes[] = {
    battery_level_0_symbolic_size, battery_level_10_symbolic_size, battery_level_20_symbolic_size, battery_level_30_symbolic_size,
    battery_level_40_symbolic_size, battery_level_50_symbolic_size, battery_level_60_symbolic_size, battery_level_70_symbolic_size,
    battery_level_80_symbolic_size, battery_level_90_symbolic_size, battery_level_100_symbolic_size,
    battery_level_0_charging_symbolic_size, battery_level_10_charging_symbolic_size, battery_level_20_charging_symbolic_size,
    battery_level_30_charging_symbolic_size, battery_level_40_charging_symbolic_size, battery_level_50_charging_symbolic_size,
    battery_level_60_charging_symbolic_size, battery_level_70_charging_symbolic_size, battery_level_80_charging_symbolic_size,
    battery_level_90_charging_symbolic_size, battery_level_100_charged_symbolic_size, 0
};

static int battery_icon_lookup[11][3] = { { -1 } };
static GdkPixbuf *battery_pixbuf_cache[11][3] = {0};
static char battery_name[16] = {0};
static int battery_fd_capacity = -1;
static int battery_fd_status = -1;
static char backlight_directory[PATH_SIZE] = {0};
static int backlight_max_brightness = 0;
static int backlight_current_brightness = 0;
static int backlight_fd_brightness = -1;
static uint8_t battery_percentage = 0;
static uint8_t battery_charging = 0;
static GtkStatusIcon *status_icon = NULL;
static gboolean warn_batt = FALSE;
static char tooltip_buffer[TOOLTIP_BUFFER_SIZE] = {0};



static inline void pregen_tooltip(void) {
    GET_BATTERY_STATUS();
    char *p = tooltip_buffer;
    const char *label = "Battery: ";
    while (*label) *p++ = *label++;
    int percent = battery_percentage;
    if (percent == 0) {
        *p++ = '0';
    } else {
        int tmp = percent, digits = 0;
        char num[3];
        while (tmp) { num[digits++] = '0' + (tmp % 10); tmp /= 10; }
        for (int i = digits - 1; i >= 0; --i) *p++ = num[i];
    }
    *p++ = '%';
    *p++ = ',';
    *p++ = ' ';
    const char *state = (battery_charging == 2) ? "Full" : (battery_charging == 1) ? "Charging" : "Discharging";
    const char *s = state;
    while (*s) *p++ = *s++;
    *p = '\0';
}


static inline void initialize_icon_lookup_table(void) {
    for (int i = 0; embedded_icon_names[i] != NULL; i++) {
        const char *name = embedded_icon_names[i];
        size_t len = 0;
        while (name[len]) ++len;
        if (memcmp(name, "battery-level-100-charged-symbolic", 31) == 0) {
            battery_icon_lookup[10][2] = i;
            continue;
        }
        int charging = (len >= 18 && memcmp(name + len - 18, "-charging-symbolic", 18) == 0) ? 1 : 0;
        if (memcmp(name, "battery-level-", 14) == 0) {
            int n = 0, p = 14;
            while (name[p] >= '0' && name[p] <= '9') n = n*10 + (name[p++] - '0');
            if (n >= 0 && n <= 100) battery_icon_lookup[n/10][charging] = i;
        }
    }
}

static GdkPixbuf *load_battery_icon(int level, int charging) {
    int state = 0;
    level = level / 10;
    if (level > 10) level = 10;
    if (level < 0) level = 0;
    if (charging == 2 || level == 10) {
        state = 2;
        level = 10;
    } else if (charging == 1) {
        state = 1;
    }
    GdkPixbuf *pixbuf = battery_pixbuf_cache[level][state];
    if (pixbuf) {
        return pixbuf;
    }
    int icon_idx = battery_icon_lookup[level][state];
    if (icon_idx < 0) return NULL;
    GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader, embedded_icon_data[icon_idx], embedded_icon_sizes[icon_idx], NULL);
    gdk_pixbuf_loader_close(loader, NULL);
    pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    if (pixbuf) {
        battery_pixbuf_cache[level][state] = g_object_ref(pixbuf);
    }
    g_object_unref(loader);
    return pixbuf;
}

static inline gboolean initialize_backlight(void) {
    const char *base_path = "/sys/class/backlight/";
    char path[PATH_SIZE];
    DIR *dir = opendir(base_path);
    if (!dir) return FALSE;
    struct dirent *entry;
    backlight_directory[0] = '\0';
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;
        size_t base_len = sizeof("/sys/class/backlight/") - 1;
        size_t name_len = 0;
        while (entry->d_name[name_len] && base_len + name_len < PATH_SIZE - 1) ++name_len;
        memcpy(backlight_directory, base_path, base_len);
        memcpy(backlight_directory + base_len, entry->d_name, name_len);
        backlight_directory[base_len + name_len] = '\0';
        break;
    }
    closedir(dir);
    if (!backlight_directory[0]) return FALSE;
    size_t dir_len = 0;
    while (backlight_directory[dir_len] && dir_len < PATH_SIZE - 15) ++dir_len;
    memcpy(path, backlight_directory, dir_len);
    memcpy(path + dir_len, "/max_brightness", 15);
    path[dir_len + 15] = '\0';
    int fd = open(path, O_RDONLY);
    if (fd < 0) { backlight_directory[0] = '\0'; return FALSE; }
    char buffer[8];
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        backlight_max_brightness = FAST_PARSE_INT(buffer, n);
    }
    close(fd);
    snprintf(path, PATH_SIZE, "%s/brightness", backlight_directory);
    backlight_fd_brightness = open(path, O_RDWR);
    if (backlight_fd_brightness < 0) { backlight_directory[0] = '\0'; return FALSE; }
    return READ_CURRENT_BRIGHTNESS();
}



static inline void find_battery_name(void) {
    DIR *dir = opendir("/sys/class/power_supply/");
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;
        char path[PATH_SIZE];
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type", entry->d_name);
        int fd = open(path, O_RDONLY);
        if (fd >= 0) {
            char type[16];
            ssize_t n = read(fd, type, sizeof(type) - 1);
            close(fd);
            if (n > 0 && type[0] == 'B' && type[1] == 'a') {
                memcpy(battery_name, entry->d_name, sizeof(battery_name));
                snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", battery_name);
                battery_fd_capacity = open(path, O_RDONLY);
                snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", battery_name);
                battery_fd_status = open(path, O_RDONLY);
                break;
            }
        }
    }
    closedir(dir);
}




static void show_low_battery_warning(void) {
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
        "Low Battery\nBattery level is critically low. Please connect to a power source."
    );
    gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    warn_batt = TRUE;
}



static gboolean hide_popup_timeout(gpointer window) {
    if (slider_active) {
        slider_active = FALSE;
        return G_SOURCE_CONTINUE;
    }
    if (window && gtk_widget_get_visible(GTK_WIDGET(window))) {
        gtk_widget_destroy(GTK_WIDGET(window));
    }
    return G_SOURCE_REMOVE;
}



static void slider_changed(GtkRange *range, gpointer user_data) {
    if (!backlight_directory[0] || backlight_fd_brightness < 0) return;
    double value = gtk_range_get_value(range);
    int brightness = (int)((value / 100.0) * backlight_max_brightness);
    if (brightness < 1) brightness = 1;
    char buffer[6];
    char *p = buffer + sizeof(buffer) - 2;
    *p-- = '\n';
    int b = brightness;
    if (b == 0) {
        *p-- = '0';
    } else {
        while (b > 0) {
            *p-- = '0' + (b % 10);
            b /= 10;
        }
    }
    p++;
    int len = buffer + sizeof(buffer) - p - 1;
    memmove(buffer, p, len);
    pwrite(backlight_fd_brightness, buffer, len, 0);
    backlight_current_brightness = brightness;
    slider_active = TRUE;
}



static void show_slider_popup(GtkStatusIcon *icon, guint button, guint activate_time, gpointer user_data) {
    GtkWidget *popup_window = gtk_window_new(GTK_WINDOW_POPUP);
    GtkWidget *vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(popup_window), vbox);
    GtkWidget *acpi_label = gtk_label_new(tooltip_buffer);
    gtk_box_pack_start(GTK_BOX(vbox), acpi_label, FALSE, FALSE, 2);
    if (backlight_directory[0] && backlight_fd_brightness >= 0) {
        READ_CURRENT_BRIGHTNESS();
        GtkWidget *slider = gtk_hscale_new_with_range(0, 100, 1);
        gtk_widget_set_size_request(slider, 200, -1);
        g_signal_connect(slider, "value-changed", G_CALLBACK(slider_changed), NULL);
        if (backlight_max_brightness > 0) {
            double slider_value = (backlight_current_brightness * 100.0) / backlight_max_brightness;
            gtk_range_set_value(GTK_RANGE(slider), slider_value);
        }
        gtk_box_pack_start(GTK_BOX(vbox), slider, TRUE, TRUE, 2);
    }
    const char *info = gtk_label_get_text(GTK_LABEL(acpi_label));
    int info_len = 0;
    while (info[info_len]) ++info_len;
    int popup_width = info_len * 8 + 20;
    if (popup_width < 320) popup_width = 320;
    gtk_widget_set_size_request(popup_window, popup_width, -1);
    gtk_widget_show_all(popup_window);
    GdkScreen *screen = gdk_screen_get_default();
    int screen_width = gdk_screen_get_width(screen);
    int screen_height = gdk_screen_get_height(screen);
    int y = backlight_directory[0] ? screen_height - 110 : screen_height - 60;
    gtk_window_move(GTK_WINDOW(popup_window), screen_width - popup_width - 10, y);
    g_timeout_add_seconds(3, hide_popup_timeout, popup_window);
}



static inline void update_battery_icon(void) {
    pregen_tooltip();
    int level = battery_percentage;
    if (battery_percentage <= 15 && !battery_charging && !warn_batt) {
        show_low_battery_warning();
    }
    if (battery_percentage > 16 || battery_charging) {
        warn_batt = FALSE;
    }
    GdkPixbuf *pixbuf = load_battery_icon(level, battery_charging);
    if (pixbuf) {
        gtk_status_icon_set_from_pixbuf(status_icon, pixbuf);
    }
    gtk_status_icon_set_tooltip_text(status_icon, tooltip_buffer);
}


static gboolean update_battery_status(gpointer user_data) {
    update_battery_icon();
    return G_SOURCE_CONTINUE;
}


static void cleanup(void) {
    if (battery_fd_capacity >= 0) close(battery_fd_capacity);
    if (battery_fd_status >= 0) close(battery_fd_status);
    if (backlight_fd_brightness >= 0) close(backlight_fd_brightness);
    for (int i = 0; i < 11; ++i)
        for (int j = 0; j < 3; ++j)
            if (battery_pixbuf_cache[i][j]) g_object_unref(battery_pixbuf_cache[i][j]);
}


int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    initialize_icon_lookup_table();
    initialize_backlight();
    find_battery_name();
    status_icon = gtk_status_icon_new();
    gtk_status_icon_set_visible(status_icon, TRUE);
    g_signal_connect(status_icon, "popup-menu", G_CALLBACK(show_slider_popup), NULL);
    g_signal_connect(status_icon, "activate", G_CALLBACK(show_slider_popup), NULL);
    pregen_tooltip();
    update_battery_icon();
    g_timeout_add_seconds(5, update_battery_status, NULL);
    g_atexit(cleanup);
    gtk_main();
    return 0;
}
