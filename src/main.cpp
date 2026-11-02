#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WireGuard-ESP32.h>

Preferences prefs;
WebServer server(80);
WireGuard wg;

// ডিফল্ট ভ্যালু
String sta_ssid = "";
String sta_pass = "";
String ap_ssid  = "ESP32_Extender";
String ap_pass  = "12345678";

String wg_private = "";
String wg_public  = "";
String wg_endpoint = "";
String wg_ip = "10.8.0.2";
int    wg_port = 51820;

bool wg_running = false;

// ========== সেটিংস লোড/সেভ ==========
void loadSettings() {
  prefs.begin("ext", true);
  sta_ssid    = prefs.getString("sta_ssid", "");
  sta_pass    = prefs.getString("sta_pass", "");
  ap_ssid     = prefs.getString("ap_ssid", "ESP32_Extender");
  ap_pass     = prefs.getString("ap_pass", "12345678");
  wg_private  = prefs.getString("wg_priv", "");
  wg_public   = prefs.getString("wg_pub", "");
  wg_endpoint = prefs.getString("wg_end", "");
  wg_ip       = prefs.getString("wg_ip", "10.8.0.2");
  wg_port     = prefs.getInt("wg_port", 51820);
  prefs.end();
}

void saveSettings() {
  prefs.begin("ext", false);
  prefs.putString("sta_ssid", sta_ssid);
  prefs.putString("sta_pass", sta_pass);
  prefs.putString("ap_ssid", ap_ssid);
  prefs.putString("ap_pass", ap_pass);
  prefs.putString("wg_priv", wg_private);
  prefs.putString("wg_pub", wg_public);
  prefs.putString("wg_end", wg_endpoint);
  prefs.putString("wg_ip", wg_ip);
  prefs.putInt("wg_port", wg_port);
  prefs.end();
}

// ========== ওয়েব পেজ ==========
void handleRoot() {
  String html = R"====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 Extender</title>
  <style>
    body{font-family:Arial;background:#0f0f0f;color:#eee;margin:0;padding:16px}
    .box{background:#1c1c1c;padding:14px;border-radius:10px;margin-bottom:14px}
    input,button{width:100%;padding:10px;margin:6px 0;border:none;border-radius:6px;box-sizing:border-box}
    input{background:#2a2a2a;color:#fff}
    button{background:#0d6efd;color:#fff;font-weight:bold}
    h3{margin:0 0 10px;color:#4fc3f7}
    .ok{color:#4caf50}
    .err{color:#f44336}
  </style>
</head>
<body>
  <h2>ESP32 Extender + WireGuard</h2>

  <div class="box">
    <h3>স্ট্যাটাস</h3>
    <div>STA: )====";

  html += (WiFi.status() == WL_CONNECTED) ? "<span class='ok'>Connected</span>" : "<span class='err'>Disconnected</span>";
  html += "<br>STA IP: " + WiFi.localIP().toString();
  html += "<br>AP IP: " + WiFi.softAPIP().toString();
  html += "<br>WireGuard: ";
  html += wg_running ? "<span class='ok'>Running</span>" : "<span class='err'>Stopped</span>";
  html += R"====(
  </div>

  <div class="box">
    <h3>হোম ওয়াইফাই (STA)</h3>
    <form action="/save" method="POST">
      <input name="sta_ssid" placeholder="Home WiFi Name" value=")====" + sta_ssid + R"====(">
      <input name="sta_pass" type="password" placeholder="Home WiFi Password" value=")====" + sta_pass + R"====(">
      
      <h3 style="margin-top:18px">এক্সটেন্ডার ওয়াইফাই (AP)</h3>
      <input name="ap_ssid" placeholder="Extender Name" value=")====" + ap_ssid + R"====(">
      <input name="ap_pass" type="password" placeholder="Extender Password" value=")====" + ap_pass + R"====(">

      <h3 style="margin-top:18px">WireGuard VPN</h3>
      <input name="wg_ip" placeholder="Tunnel IP (10.8.0.2)" value=")====" + wg_ip + R"====(">
      <input name="wg_private" placeholder="Private Key" value=")====" + wg_private + R"====(">
      <input name="wg_public" placeholder="Peer Public Key" value=")====" + wg_public + R"====(">
      <input name="wg_endpoint" placeholder="Endpoint (vpn.example.com)" value=")====" + wg_endpoint + R"====(">
      <input name="wg_port" type="number" placeholder="Port (51820)" value=")====" + String(wg_port) + R"====(">

      <button type="submit">সেভ করো ও রিবুট</button>
    </form>
  </div>
</body>
</html>
)====";

  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("sta_ssid"))    sta_ssid    = server.arg("sta_ssid");
  if (server.hasArg("sta_pass"))    sta_pass    = server.arg("sta_pass");
  if (server.hasArg("ap_ssid"))     ap_ssid     = server.arg("ap_ssid");
  if (server.hasArg("ap_pass"))     ap_pass     = server.arg("ap_pass");
  if (server.hasArg("wg_private"))  wg_private  = server.arg("wg_private");
  if (server.hasArg("wg_public"))   wg_public   = server.arg("wg_public");
  if (server.hasArg("wg_endpoint")) wg_endpoint = server.arg("wg_endpoint");
  if (server.hasArg("wg_ip"))       wg_ip       = server.arg("wg_ip");
  if (server.hasArg("wg_port"))     wg_port     = server.arg("wg_port").toInt();

  saveSettings();
  server.send(200, "text/html", "<h3>সেভ হয়েছে। রিবুট হচ্ছে...</h3>");
  delay(1000);
  ESP.restart();
}

// ========== সেটআপ ==========
void setup() {
  Serial.begin(115200);
  delay(500);

  loadSettings();

  WiFi.mode(WIFI_AP_STA);

  // SoftAP আগে চালু (যাতে অ্যাডমিন প্যানেল পাওয়া যায়)
  WiFi.softAP(ap_ssid.c_str(), ap_pass.c_str());
  Serial.println("AP: " + WiFi.softAPIP().toString());

  // STA কানেক্ট
  if (sta_ssid.length() > 0) {
    WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
    Serial.print("STA connecting");
    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 30) {
      delay(500);
      Serial.print(".");
      t++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("STA IP: " + WiFi.localIP().toString());
    }
  }

  // NTP (WireGuard-এর জন্য দরকার)
  configTime(6 * 3600, 0, "pool.ntp.org", "time.google.com");
  time_t now = time(nullptr);
  int ntp_try = 0;
  while (now < 100000 && ntp_try < 20) {
    delay(300);
    now = time(nullptr);
    ntp_try++;
  }

  // WireGuard
  if (wg_private.length() > 20 && wg_public.length() > 20 && wg_endpoint.length() > 3) {
    IPAddress lip;
    lip.fromString(wg_ip);
    if (wg.begin(lip, wg_private.c_str(), wg_endpoint.c_str(), wg_public.c_str(), (uint16_t)wg_port)) {
      wg_running = true;
      Serial.println("WireGuard started");
    } else {
      Serial.println("WireGuard failed");
    }
  }

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("Web UI → http://192.168.4.1");
}

void loop() {
  server.handleClient();
  delay(2);
}
