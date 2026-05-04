
//   GET /
//   GET /download?f=<path>
//   GET /delete?f=<path>
//   GET /preview?f=<path>

#include "wifi_server.h"
#include "sd_logger.h"
#include "config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>

static const char*    AP_SSID     = "DepthMapper";
static const char*    AP_PASSWORD = "12345678";
static const IPAddress AP_IP(192, 168, 4, 1);

static WebServer server(80);
static bool      server_running = false;

static const char* PAGE_STYLE =
    "<style>"
    "body{font-family:monospace;max-width:860px;margin:40px auto;padding:0 16px;"
         "background:#111;color:#e0e0e0}"
    "h1{color:#4fc3f7}"
    "a{color:#80cbc4;text-decoration:none}"
    "a:hover{text-decoration:underline}"
    "table{width:100%;border-collapse:collapse;margin-top:16px}"
    "th{background:#1e1e1e;padding:8px 12px;text-align:left;border-bottom:1px solid #333}"
    "td{padding:6px 12px;border-bottom:1px solid #1e1e1e}"
    "tr:hover td{background:#1a2a2a}"
    ".btn{display:inline-block;padding:4px 10px;border-radius:4px;font-size:.85em;"
         "margin-right:4px}"
    ".dl{background:#1b5e20;color:#a5d6a7}"
    ".pr{background:#1a237e;color:#90caf9}"
    ".de{background:#b71c1c;color:#ef9a9a}"
    "</style>";

static void listDir(File dir, const String& base) {
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;

        String path  = base + "/" + entry.name();
        String label = entry.name();

        if (entry.isDirectory()) {
            listDir(entry, path);
        } else {
            long  bytes = entry.size();
            String size;
            if      (bytes < 1024)    size = String(bytes) + " B";
            else if (bytes < 1048576) size = String(bytes / 1024) + " KB";
            else                      size = String(bytes / 1048576.0, 1) + " MB";

            bool previewable = path.endsWith(".txt") || path.endsWith(".csv")
                             || path.endsWith(".log");

            server.sendContent("<tr><td>" + label + "</td><td>" + size + "</td><td>");
            server.sendContent("<a class='btn dl' href='/download?f=" + path + "'>Download</a>");
            if (previewable)
                server.sendContent("<a class='btn pr' href='/preview?f=" + path + "'>Preview</a>");
            server.sendContent("<a class='btn de' href='/delete?f=" + path
                         + "' onclick=\"return confirm('Delete " + label + "?')\">Delete</a>");
            server.sendContent("</td></tr>\n");
        }
        entry.close();
    }
}

static void handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    server.sendContent("<!DOCTYPE html><html><head>"
                 "<meta charset='utf-8'>"
                 "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                 "<title>DepthMapper</title>");
    server.sendContent(PAGE_STYLE);
    server.sendContent("</head><body>");
    server.sendContent("<h1>DepthMapper &mdash; SD Card</h1>");

    if (!sd_is_ok()) {
        server.sendContent("<p style='color:#ef9a9a'>&#x26A0; SD card not available.</p>");
    } else {
        server.sendContent("<table>"
                     "<tr><th>File</th><th>Size</th><th>Actions</th></tr>\n");
        File root = SD.open("/");
        listDir(root, "");
        root.close();
        server.sendContent("</table>");
    }

    server.sendContent("<p style='margin-top:32px;color:#555'>"
                 "SSID: <b style='color:#80cbc4'>DepthMapper</b> &nbsp;|&nbsp; "
                 "IP: <b style='color:#80cbc4'>192.168.4.1</b><br>"
                 "<small>Press the UP button on the device to exit Wi-Fi mode.</small>"
                 "</p>");
    server.sendContent("</body></html>");

    server.sendContent(""); 
}

static void handleDownload() {
    if (!server.hasArg("f")) { server.send(400, "text/plain", "Missing ?f="); return; }
    String path = server.arg("f");
    if (!SD.exists(path)) { server.send(404, "text/plain", "Not found"); return; }

    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) { server.send(500, "text/plain", "Cannot open"); return; }

    String fname = path;
    int slash = fname.lastIndexOf('/');
    if (slash >= 0) fname = fname.substring(slash + 1);

    server.sendHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
    server.sendHeader("Content-Length", String(f.size()));
    server.streamFile(f, "application/octet-stream");
    f.close();
}

static void handlePreview() {
    if (!server.hasArg("f")) { server.send(400, "text/plain", "Missing ?f="); return; }
    String path = server.arg("f");
    if (!SD.exists(path)) { server.send(404, "text/plain", "Not found"); return; }

    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) { server.send(500, "text/plain", "Cannot open"); return; }

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    server.sendContent("<!DOCTYPE html><html><head><meta charset='utf-8'>"
                 "<title>Preview</title>");
    server.sendContent(PAGE_STYLE);
    server.sendContent("</head><body>");
    server.sendContent("<p><a href='/'>&#x2190; Back</a> &nbsp;"
                 "<a class='btn dl' href='/download?f=" + path + "'>Download</a></p>"
                 "<h1>" + path + "</h1><pre>");

    int lines = 0;
    while (f.available() && lines < 200) {
        String line = f.readStringUntil('\n');
        line.replace("&", "&amp;");
        line.replace("<", "&lt;");
        line.replace(">", "&gt;");
        server.sendContent(line + "\n");
        lines++;
    }
    if (f.available())
        server.sendContent("\n... (truncated &mdash; download for full file)");

    server.sendContent("</pre></body></html>");
    f.close();

    server.sendContent("");
}

static void handleDelete() {
    if (!server.hasArg("f")) { server.send(400, "text/plain", "Missing ?f="); return; }
    String path = server.arg("f");
    if (SD.exists(path)) SD.remove(path);
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "Deleted");
}

static void handleNotFound() {
    server.send(404, "text/plain", "Not found");
}


void wifi_server_start() {
    if (server_running) return;

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID, AP_PASSWORD[0] ? AP_PASSWORD : nullptr);

    Serial.print("[WiFi] AP up: ");
    Serial.print(AP_SSID);
    Serial.print("  ");
    Serial.println(WiFi.softAPIP());

    server.on("/",         handleRoot);
    server.on("/download", handleDownload);
    server.on("/preview",  handlePreview);
    server.on("/delete",   handleDelete);
    server.onNotFound(handleNotFound);
    server.begin();

    server_running = true;
    Serial.println("[WiFi] HTTP server started");
}

void wifi_server_stop() {
    if (!server_running) return;
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    server_running = false;
    Serial.println("[WiFi] AP stopped, radio off");
}

void wifi_server_handle() {
    if (server_running) server.handleClient();
}

int wifi_server_client_count() {
    return server_running ? (int)WiFi.softAPgetStationNum() : 0;
}
