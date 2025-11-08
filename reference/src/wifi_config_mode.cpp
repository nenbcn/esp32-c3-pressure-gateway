// wifi_config_mode.cpp
#include "config.h"
#include "wifi_config_mode.h"
#include "Vector.h"


AsyncWebServer server(80);
String g_wifiOptions = ""; // Global variable to store detected SSIDs

// Initialize WiFi Config Mode
void initializeWiFiConfigMode() {
    Log::info("Entering initializeWiFiConfigMode()...");

    // 1️⃣ First, scan networks in STA mode before activating the AP
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Log::info("Scanning available WiFi networks...");

    int n = WiFi.scanNetworks();
    if (n > 0) {
        Log::info("Found %d networks.", n);
        Vector<String> found;
        g_wifiOptions = "<option value='manual'>Enter SSID manually</option>";
        for (int i = 0; i < n; ++i)
        {
            String ssid = WiFi.SSID(i);
            if (!found.contains(ssid))
            {
                g_wifiOptions += "<option value='" + WiFi.SSID(i) + "'>";
                g_wifiOptions += WiFi.SSID(i);
                g_wifiOptions += "</option>";
            } else {
                Log::info("Found duplicate entry: %s", ssid);
            }
        }
    } else {
        Log::warn("No Wi-Fi networks found.");
        g_wifiOptions = "<option value='manual'>Enter SSID manually</option>";
        g_wifiOptions += "<option value=''>No networks found</option>";
    }

    // 2️⃣ Second, start the AP
    WiFi.mode(WIFI_AP);
    if (WiFi.softAP(AP_SSID, AP_PASSWORD)) {
        Log::info("Access Point started with SSID: %s", AP_SSID);
        Log::info("IP Address: %s", WiFi.softAPIP().toString().c_str());
        notifySystemState(EVENT_WIFI_CONFIG_STARTED);
    } else {
        Log::error("Failed to start Access Point.");
        notifySystemState(EVENT_WIFI_CONFIG_FAILED);
        return;
    }

    // 3️⃣ Third, configure the web server routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Log::info("HTTP Request received at /");

        String html = R"rawliteral(
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <title>MICA Gateway WiFi Config</title>
                <style>
                    body { font-family: Arial, sans-serif; text-align: center; margin: 40px; }
                    h1 { color: #333; }
                    form { display: flex; flex-direction: column; align-items: center; }
                    label { font-size: 18px; margin: 10px 0; }
                    select, input { width: 80%; max-width: 300px; padding: 10px; margin: 10px 0; font-size: 16px; }
                    .button { width: 80%; max-width: 320px; padding: 15px; font-size: 18px; margin: 10px 0; border: none; cursor: pointer; border-radius: 8px; }
                    .save-button { background-color: #28a745; color: white; }
                    .refresh-button { background-color: #007bff; color: white; }
                </style>
                <script>
                    function toggleSSIDInput() {
                        var ssidSelect = document.getElementById("ssid");
                        var ssidInput = document.getElementById("ssid_manual");
                        if (ssidSelect.value === "manual") {
                            ssidInput.style.display = "block";
                        } else {
                            ssidInput.style.display = "none";
                        }
                    }
                </script>
            </head>
            <body>
                <h1>WiFi Config Mode</h1>
                <form action="/save" method="POST">
                    <label for="ssid">Select WiFi Network:</label>
                    <select id="ssid" name="ssid" onchange="toggleSSIDInput()">
        )rawliteral";

        html += g_wifiOptions;  // Add detected SSIDs

        html += R"rawliteral(
                    </select>
                    <input type="text" id="ssid_manual" name="ssid_manual" placeholder="Enter SSID" style="display:none;">
                    <label for="password">Enter WiFi Password:</label>
                    <input type="password" id="password" name="password" placeholder="Enter WiFi Password">
                    
                    <button type="submit" class="button save-button">💾 Save Configuration</button>
                </form>
            </body>
            </html>
        )rawliteral";

        request->send(200, "text/html", html);
    });

    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
        String ssid, password;

        if (request->hasParam("ssid", true)) {
            ssid = request->getParam("ssid", true)->value();
        }
        if (request->hasParam("ssid_manual", true) && !request->getParam("ssid_manual", true)->value().isEmpty()) {
            ssid = request->getParam("ssid_manual", true)->value();
        }
        if (request->hasParam("password", true)) {
            password = request->getParam("password", true)->value();
        }

        if (ssid.isEmpty() || password.length() < 8) {
            request->send(400, "text/html", "<h1>Invalid credentials. Please try again.</h1>");
            Log::warn("Invalid credentials received via web interface.");
            notifySystemState(EVENT_WIFI_CONFIG_FAILED);
            return;
        }

        if (saveCredentials(ssid, password)) {
            Log::info("Credentials saved in EEPROM.");
            request->send(200, "text/html", "<h1>Configuration Saved. Restarting...</h1>");
            notifySystemState(EVENT_WIFI_CONFIG_SAVED);
            deactivateWiFiConfigMode();
        } else {
            Log::error("Failed to save credentials in EEPROM.");
            request->send(500, "text/html", "<h1>Failed to save credentials. Please try again.</h1>");
            notifySystemState(EVENT_WIFI_CONFIG_FAILED);
        }
    });

    // 4️⃣ Fourth, start the web server
    Log::info("Starting Web Server...");
    server.begin();
    Log::info("Web Server started successfully.");
    Log::info("Please, enter the following URL in your browser: http://192.168.4.1");
}

void wifiConfigModeTask(void *pvParameters) {
    static bool isAPActive = false;

    Log::info("WiFi Config Mode Task started...");

    while (true) {
        if (getSystemState() == SYSTEM_STATE_CONFIG_MODE) {
            Log::info("Entering CONFIG_MODE.");

            if (!isAPActive) {
                Log::info("Initializing WiFi Config Mode (AP & Web Server)...");
                initializeWiFiConfigMode();
                isAPActive = true;
            }

            // Wait until the system state is no longer CONFIG_MODE
            while (getSystemState() == SYSTEM_STATE_CONFIG_MODE) {
                vTaskDelay(pdMS_TO_TICKS(500)); // Reduced to avoid overloading the CPU
            }

            Log::info("Exiting CONFIG_MODE. Cleaning up WiFi Config.");
            deactivateWiFiConfigMode();
            isAPActive = false;  // Allow reactivation in the future
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // State check without overloading the CPU
    }
}

// Deactivate WiFi Config Mode
void deactivateWiFiConfigMode() {
    Log::info("Deactivating WiFi Config Mode...");
    server.end();
    WiFi.softAPdisconnect(true);
    Log::info("Web server stopped and AP disabled.");
    notifySystemState(EVENT_WIFI_CONFIG_STOPPED); // Notify that the configuration mode stopped
    vTaskDelay(pdMS_TO_TICKS(2000)); // Ensure the AP is disabled
    ESP.restart();
}