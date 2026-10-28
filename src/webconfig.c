#include "webconfig.h"
#include "wifiap.h"
#include "httpserver.h"

#include <zephyr/kernel.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define AP_CHANNEL  6
#define HTTP_PORT   80

static settings_t *cfg;
static webconfig_callback_t on_save;

/* ------------------------------------------------------------------ */
/* URL decoding                                                       */
/* ------------------------------------------------------------------ */

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char *dst, const char *src, size_t dst_size)
{
    size_t di = 0;

    while (*src && di < dst_size - 1) {
        if (*src == '%' && src[1] && src[2]) {
            int hi = hex_val(src[1]);
            int lo = hex_val(src[2]);
            if (hi >= 0 && lo >= 0) {
                dst[di++] = (char)(hi * 16 + lo);
                src += 3;
                continue;
            }
        }
        if (*src == '+') {
            dst[di++] = ' ';
            src++;
        } else {
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
}

/* ------------------------------------------------------------------ */
/* Form field helpers                                                  */
/* ------------------------------------------------------------------ */

static const char *find_field(const char *body, const char *name,
                              char *out, size_t out_size)
{
    size_t nlen = strlen(name);
    const char *p = body;

    while ((p = strstr(p, name)) != NULL) {
        if ((p == body || *(p - 1) == '&') && p[nlen] == '=') {
            const char *val = p + nlen + 1;
            const char *end = strchr(val, '&');
            size_t vlen = end ? (size_t)(end - val) : strlen(val);

            char encoded[128];
            if (vlen >= sizeof(encoded)) vlen = sizeof(encoded) - 1;
            memcpy(encoded, val, vlen);
            encoded[vlen] = '\0';

            url_decode(out, encoded, out_size);
            return val;
        }
        p += nlen;
    }
    out[0] = '\0';
    return NULL;
}

static bool field_present(const char *body, const char *name)
{
    char tmp[4];
    return find_field(body, name, tmp, sizeof(tmp)) != NULL;
}

static uint8_t field_uint8(const char *body, const char *name, uint8_t def)
{
    char tmp[8];
    if (!find_field(body, name, tmp, sizeof(tmp))) return def;
    int v = atoi(tmp);
    return (uint8_t)((v < 0) ? 0 : (v > 255) ? 255 : v);
}

static int field_int(const char *body, const char *name, int def)
{
    char tmp[8];
    if (!find_field(body, name, tmp, sizeof(tmp))) return def;
    return atoi(tmp);
}

/* ------------------------------------------------------------------ */
/* Form parsing                                                       */
/* ------------------------------------------------------------------ */

static const uint8_t brightness_map[] = {0, 0, 1, 3, 8, 15};

static void parse_form(const char *body)
{
    find_field(body, "ssid", cfg->wifi_ssid, sizeof(cfg->wifi_ssid));
    find_field(body, "pass", cfg->wifi_pass, sizeof(cfg->wifi_pass));

    cfg->fahrenheit  = field_present(body, "fahrenheit");
    cfg->time_24h    = field_present(body, "time_24h");
    cfg->hourly_beep = field_present(body, "hourly_beep");

    for (int i = 0; i < SETTINGS_MAX_ALARMS; i++) {
        char key[16];

        snprintf(key, sizeof(key), "al%d_h", i);
        cfg->alarms[i].hour = field_uint8(body, key, 0);
        if (cfg->alarms[i].hour > 23) cfg->alarms[i].hour = 0;

        snprintf(key, sizeof(key), "al%d_m", i);
        cfg->alarms[i].minute = field_uint8(body, key, 0);
        if (cfg->alarms[i].minute > 59) cfg->alarms[i].minute = 0;

        snprintf(key, sizeof(key), "al%d_en", i);
        cfg->alarms[i].enabled = field_present(body, key);
    }

    find_field(body, "ntp_server", cfg->ntp_server, sizeof(cfg->ntp_server));

    int utc_h = field_int(body, "utc_offset", 0);
    if (utc_h < -12) utc_h = -12;
    if (utc_h > 14) utc_h = 14;
    cfg->utc_offset_sec = (int32_t)utc_h * 3600;

    int blvl = field_int(body, "brightness", 3);
    if (blvl < 1) blvl = 1;
    if (blvl > 5) blvl = 5;
    cfg->brightness = brightness_map[blvl];

    char color[8];
    if (find_field(body, "led_color", color, sizeof(color))) {
        cfg->led_r = cfg->led_g = cfg->led_b = false;
        if (strcmp(color, "red") == 0)        { cfg->led_r = true; }
        else if (strcmp(color, "green") == 0) { cfg->led_g = true; }
        else if (strcmp(color, "blue") == 0)  { cfg->led_b = true; }
        else { cfg->led_r = cfg->led_g = cfg->led_b = true; }
    }
}

/* ------------------------------------------------------------------ */
/* HTML page generation                                               */
/* ------------------------------------------------------------------ */

static const char PAGE_HEAD[] =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Connection: close\r\n\r\n"
    "<!DOCTYPE html><html><head>"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>HU-058 Clock</title>"
    "<style>"
    "body{font-family:sans-serif;margin:1em;max-width:480px}"
    "fieldset{margin-bottom:1em}"
    "label{display:block;margin:0.3em 0}"
    "input[type=text],input[type=password],input[type=number],select"
    "{width:100%;padding:0.4em;box-sizing:border-box}"
    ".alarm{display:flex;gap:0.5em;align-items:center;margin:0.3em 0}"
    ".alarm input[type=number]{width:4em}"
    ".color-group label{display:inline;margin-right:1em}"
    "button{padding:0.6em 2em;font-size:1em}"
    "</style></head><body>"
    "<h2>Clock Settings</h2>"
    "<form method=\"POST\" action=\"/\">";

static const char PAGE_TAIL[] =
    "<button type=\"submit\">Save</button>"
    "</form></body></html>";

static const char PAGE_SAVED[] =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Connection: close\r\n\r\n"
    "<!DOCTYPE html><html><head>"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>Saved</title></head><body>"
    "<h2>Settings saved!</h2>"
    "<p><a href=\"/\">Back</a></p>"
    "</body></html>";

static void send_config_page(int fd)
{
    char buf[512];
    int n;

    httpserver_send(fd, PAGE_HEAD, sizeof(PAGE_HEAD) - 1);

    /* WiFi */
    n = snprintf(buf, sizeof(buf),
        "<fieldset><legend>WiFi</legend>"
        "<label>SSID<input type=\"text\" name=\"ssid\" "
        "maxlength=\"%d\" value=\"%s\"></label>"
        "<label>Password<input type=\"password\" name=\"pass\" "
        "maxlength=\"%d\" value=\"%s\"></label>"
        "</fieldset>",
        SETTINGS_SSID_MAX, cfg->wifi_ssid,
        SETTINGS_PASS_MAX, cfg->wifi_pass);
    httpserver_send(fd, buf, n);

    /* Display */
    n = snprintf(buf, sizeof(buf),
        "<fieldset><legend>Display</legend>"
        "<label><input type=\"checkbox\" name=\"fahrenheit\"%s>"
        " Temperature in &deg;F</label>"
        "<label><input type=\"checkbox\" name=\"time_24h\"%s>"
        " 24-hour mode</label>"
        "</fieldset>",
        cfg->fahrenheit ? " checked" : "",
        cfg->time_24h ? " checked" : "");
    httpserver_send(fd, buf, n);

    /* Hourly beep */
    n = snprintf(buf, sizeof(buf),
        "<fieldset><legend>Hourly Beep</legend>"
        "<label><input type=\"checkbox\" name=\"hourly_beep\"%s>"
        " Beep on the hour</label>"
        "</fieldset>",
        cfg->hourly_beep ? " checked" : "");
    httpserver_send(fd, buf, n);

    /* NTP */
    {
        int offset_h = (int)(cfg->utc_offset_sec / 3600);
        n = snprintf(buf, sizeof(buf),
            "<fieldset><legend>NTP</legend>"
            "<label>Server<input type=\"text\" name=\"ntp_server\" "
            "maxlength=\"%d\" value=\"%s\"></label>"
            "<label>UTC Offset (hours)"
            "<input type=\"number\" name=\"utc_offset\" "
            "min=\"-12\" max=\"14\" value=\"%d\"></label>"
            "</fieldset>",
            SETTINGS_NTP_SERVER_MAX, cfg->ntp_server,
            offset_h);
        httpserver_send(fd, buf, n);
    }

    /* LED Display — brightness */
    {
        int blvl = 3;
        if (cfg->brightness <= 0) blvl = 1;
        else if (cfg->brightness <= 1) blvl = 2;
        else if (cfg->brightness <= 3) blvl = 3;
        else if (cfg->brightness <= 8) blvl = 4;
        else blvl = 5;

        n = snprintf(buf, sizeof(buf),
            "<fieldset><legend>LED Display</legend>"
            "<label>Brightness<select name=\"brightness\">"
            "<option value=\"1\"%s>1 (dimmest)</option>"
            "<option value=\"2\"%s>2</option>"
            "<option value=\"3\"%s>3 (default)</option>"
            "<option value=\"4\"%s>4</option>"
            "<option value=\"5\"%s>5 (brightest)</option>"
            "</select></label>",
            blvl == 1 ? " selected" : "",
            blvl == 2 ? " selected" : "",
            blvl == 3 ? " selected" : "",
            blvl == 4 ? " selected" : "",
            blvl == 5 ? " selected" : "");
        httpserver_send(fd, buf, n);

        /* LED Display — color */
        const char *color = "white";
        if (cfg->led_r && !cfg->led_g && !cfg->led_b) color = "red";
        else if (!cfg->led_r && cfg->led_g && !cfg->led_b) color = "green";
        else if (!cfg->led_r && !cfg->led_g && cfg->led_b) color = "blue";

        n = snprintf(buf, sizeof(buf),
            "<label>Color</label>"
            "<div class=\"color-group\">"
            "<label><input type=\"radio\" name=\"led_color\" "
            "value=\"white\"%s> White</label>"
            "<label><input type=\"radio\" name=\"led_color\" "
            "value=\"red\"%s> Red</label>"
            "<label><input type=\"radio\" name=\"led_color\" "
            "value=\"green\"%s> Green</label>"
            "<label><input type=\"radio\" name=\"led_color\" "
            "value=\"blue\"%s> Blue</label>"
            "</div></fieldset>",
            strcmp(color, "white") == 0 ? " checked" : "",
            strcmp(color, "red") == 0 ? " checked" : "",
            strcmp(color, "green") == 0 ? " checked" : "",
            strcmp(color, "blue") == 0 ? " checked" : "");
        httpserver_send(fd, buf, n);
    }

    /* Alarms */
    httpserver_send(fd, "<fieldset><legend>Alarms</legend>", 33);
    for (int i = 0; i < SETTINGS_MAX_ALARMS; i++) {
        n = snprintf(buf, sizeof(buf),
            "<div class=\"alarm\">"
            "<span>Alarm %d</span>"
            "<input type=\"number\" name=\"al%d_h\" min=\"0\" max=\"23\" "
            "value=\"%u\">"
            "<span>:</span>"
            "<input type=\"number\" name=\"al%d_m\" min=\"0\" max=\"59\" "
            "value=\"%u\">"
            "<label><input type=\"checkbox\" name=\"al%d_en\"%s>"
            " On</label>"
            "</div>",
            i + 1,
            i, cfg->alarms[i].hour,
            i, cfg->alarms[i].minute,
            i, cfg->alarms[i].enabled ? " checked" : "");
        httpserver_send(fd, buf, n);
    }
    httpserver_send(fd, "</fieldset>", 11);

    httpserver_send(fd, PAGE_TAIL, sizeof(PAGE_TAIL) - 1);
}

/* ------------------------------------------------------------------ */
/* HTTP request handler                                               */
/* ------------------------------------------------------------------ */

static void request_handler(int fd, const http_request_t *req, void *user_data)
{
    ARG_UNUSED(user_data);

    if (strcmp(req->method, "POST") == 0 && req->body) {
        parse_form(req->body);
        if (on_save) on_save(cfg);
        httpserver_send(fd, PAGE_SAVED, sizeof(PAGE_SAVED) - 1);
    } else {
        send_config_page(fd);
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

int webconfig_start(settings_t *settings, webconfig_callback_t callback)
{
    cfg = settings;
    on_save = callback;

    int ret = wifiap_start(WEBCONFIG_AP_SSID, AP_CHANNEL);
    if (ret < 0) return ret;

    ret = httpserver_start(HTTP_PORT, request_handler, NULL);
    if (ret < 0) {
        wifiap_stop();
        return ret;
    }

    return 0;
}

void webconfig_stop(void)
{
    httpserver_stop();
    wifiap_stop();
    cfg = NULL;
    on_save = NULL;
}
