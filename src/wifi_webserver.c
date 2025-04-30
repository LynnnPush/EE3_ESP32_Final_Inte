#include "integrated_system.h"
#include "servo.h"  // Add this include for servo_move function
#include "driver/ledc.h"  // Add this for LEDC constants

static const char *TAG = "wifi-webserver";
static httpd_handle_t server = NULL;

// Global variables for RFID status
bool rfid_status_updated = false;
bool rfid_last_authorized = false;
char rfid_last_uid[50] = "None";
extern esp_timer_handle_t door_close_timer; // Reference the timer created in main.c

// HTML dashboard page with RFID and NRF24L01 functionality
static const char *html_page = "<!DOCTYPE html>\n"
"<html>\n"
"<head>"
"    <meta charset=\"UTF-8\">"
"    <title>ESP32 Integrated System</title>"
"    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
"    <style>\n"
"        body { font-family: Arial; margin: 20px; text-align: center; background-color: #f5f5f5; }\n"
"        .card { background: white; border-radius: 10px; padding: 20px; margin: 20px 0; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }\n"
"        .sensor { font-size: 24px; font-weight: bold; margin: 10px 0; }\n"
"        .temp { color: #e74c3c; }\n"
"        .humid { color: #3498db; }\n"
"        .controls button { padding: 10px 15px; margin: 5px; background-color: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; }\n"
"        .controls button:hover { background-color: #45a049; }\n"
"        .status-indicator { display: inline-block; width: 15px; height: 15px; border-radius: 50%; margin-right: 5px; }\n"
"        .on { background-color: #4CAF50; }\n" 
"        .off { background-color: #ccc; }\n"
"        .rfid-authorized { color: #4CAF50; }\n"
"        .rfid-denied { color: #e74c3c; }\n"
"        table { margin: 0 auto; border-collapse: collapse; width: 80%; }\n"
"        th, td { padding: 8px; text-align: left; border-bottom: 1px solid #ddd; }\n"
"        tr:hover {background-color: #f2f2f2;}\n"
"        .update-time { font-size: 12px; color: #666; margin-top: 10px; }\n"
"        .tabs { overflow: hidden; border: 1px solid #ccc; background-color: #f1f1f1; }\n"
"        .tabs button { background-color: inherit; float: left; border: none; outline: none; cursor: pointer; padding: 14px 16px; transition: 0.3s; }\n"
"        .tabs button:hover { background-color: #ddd; }\n"
"        .tabs button.active { background-color: #4CAF50; color: white; }\n"
"        .tabcontent { display: none; padding: 6px 12px; border: 1px solid #ccc; border-top: none; }\n"
"        #Climate { display: block; }\n"
"        .nrf-message { word-break: break-all; margin: 10px 0; padding: 10px; background-color: #f8f8f8; border-radius: 5px; text-align: left; }\n"
"        .message-input { width: 80%; padding: 10px; margin: 10px 0; }\n"
"        .data-value { font-weight: bold; }\n"
"        .message-timestamp { font-size: 12px; color: #666; margin-top: 5px; }\n"
"    </style>\n"
"</head>\n"
"<body>\n"
"    <h1>ESP32 Integrated Climate & Access Control</h1>\n"
"    \n"
"    <div class=\"tabs\">\n"
"        <button class=\"tablinks active\" onclick=\"openTab(event, 'Climate')\">Climate Control</button>\n"
"        <button class=\"tablinks\" onclick=\"openTab(event, 'RFID')\">RFID Access</button>\n"
"        <button class=\"tablinks\" onclick=\"openTab(event, 'NRF24')\">NRF24L01</button>\n"
"    </div>\n"
"    \n"
"    <div id=\"Climate\" class=\"tabcontent\">\n"
"        <div class=\"card\">\n"
"            <h2>Sensor Readings</h2>\n"
"            <div class=\"sensor temp\">Temperature: <span id=\"temp\">--</span>°C</div>\n"
"            <div class=\"sensor humid\">Humidity: <span id=\"hum\">--</span>%</div>\n"
"            <div class=\"update-time\">Last updated: <span id=\"climate-update-time\">--</span></div>\n"
"        </div>\n"
"        \n"
"        <div class=\"card\">\n"
"            <h2>Climate Control Status</h2>\n"
"            <div style=\"margin: 10px 0;\">\n"
"                <span class=\"status-indicator\" id=\"fan-indicator\"></span> Fan: <span id=\"fan\">--</span>\n"
"            </div>\n"
"            <div style=\"margin: 10px 0;\">\n"
"                <span class=\"status-indicator\" id=\"heater-indicator\"></span> Heater: <span id=\"heater\">--</span>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class=\"card\">\n"
"            <h2>Climate Controls</h2>\n"
"            <div class=\"controls\">\n"
"                <div>\n"
"                    <button onclick=\"control('fan', 'auto')\">Fan Auto</button>\n"
"                    <button onclick=\"control('fan', 'on')\">Fan On</button>\n"
"                    <button onclick=\"control('fan', 'off')\">Fan Off</button>\n"
"                </div>\n"
"                <div style=\"margin-top: 10px;\">\n"
"                    <button onclick=\"control('heater', 'auto')\">Heater Auto</button>\n"
"                    <button onclick=\"control('heater', 'on')\">Heater On</button>\n"
"                    <button onclick=\"control('heater', 'off')\">Heater Off</button>\n"
"                </div>\n"
"            </div>\n"
"        </div>\n"
"    </div>\n"
"    \n"
"    <div id=\"RFID\" class=\"tabcontent\">\n"
"        <div class=\"card\">\n"
"            <h2>RFID Access Control</h2>\n"
"            <div style=\"margin: 15px 0;\">\n"
"                <h3>Last Card Read</h3>\n"
"                <div>UID: <span id=\"last-uid\">--</span></div>\n"
"                <div>Status: <span id=\"rfid-status\">--</span></div>\n"
"            </div>\n"
"            \n"
"            <h3>Authorized Cards</h3>\n"
"            <div id=\"authorized-cards\">\n"
"                <p>Loading authorized cards...</p>\n"
"            </div>\n"
"            <div class=\"update-time\">Last updated: <span id=\"rfid-update-time\">--</span></div>\n"
"        </div>\n"
"        \n"
"        <div class=\"card\">\n"
"            <h2>Door Control</h2>\n"
"            <div class=\"controls\">\n"
"                <button onclick=\"doorControl('open')\">Open Door</button>\n"
"                <button onclick=\"doorControl('close')\">Close Door</button>\n"
"            </div>\n"
"        </div>\n"
"    </div>\n"
"    \n"
"    <div id=\"NRF24\" class=\"tabcontent\">\n"
"        <div class=\"card\">\n"
"            <h2>NRF24L01 Transceiver</h2>\n"
"            <div style=\"margin: 15px 0;\">\n"
"                <h3>Status</h3>\n"
"                <div>Mode: <span id=\"nrf-mode\">--</span></div>\n"
"                <div>Last Message: </div>\n"
"                <div class=\"nrf-message\" id=\"nrf-last-message\">\n"
"                    <div>Raw: <span id=\"nrf-raw-message\">--</span></div>\n"
"                    <div id=\"nrf-parsed-values\" style=\"display:none;\">\n"
"                        <div>Accumulated item: <span id=\"nrf-value-1\" class=\"data-value\">--</span></div>\n"
"                        <div>Current item weight: <span id=\"nrf-value-2\" class=\"data-value\">--</span></div>\n"
"                        <div>Item status: <span id=\"nrf-value-3\" class=\"data-value\">--</span></div>\n"
"                    </div>\n"
"                    <div class=\"message-timestamp\">Received: <span id=\"nrf-timestamp\">--</span></div>\n"
"                </div>\n"
"            </div>\n"
"        </div>\n"
"        \n"
"        <div class=\"card\">\n"
"            <h2>NRF24L01 Controls</h2>\n"
"            <div class=\"controls\">\n"
"                <h3>Mode</h3>\n"
"                <button onclick=\"nrfControl('mode', 'rx')\">Switch to RX Mode</button>\n"
"                <button onclick=\"nrfControl('mode', 'tx')\">Switch to TX Mode</button>\n"
"                \n"
"                <h3>Send Message (TX Mode)</h3>\n"
"                <input type=\"text\" id=\"message-input\" class=\"message-input\" placeholder=\"Enter message to send\">\n"
"                <button onclick=\"sendNRFMessage()\">Send Message</button>\n"
"                \n"
"                <h3>Quick Commands</h3>\n"
"                <button onclick=\"sendQuickCommand('OPEN_DOOR')\">Open Door</button>\n"
"                <button onclick=\"sendQuickCommand('CLOSE_DOOR')\">Close Door</button>\n"
"                <button onclick=\"sendQuickCommand('GET_TEMP')\">Get Temperature</button>\n"
"            </div>\n"
"        </div>\n"
"    </div>\n"
"    \n"
"    <script>\n"
"        // Variables to track last NRF message\n"
"        let lastNrfMessage = '';"
"        let nrfUpdateTimestamp = '';\n"

"        // Tab functionality\n"
"        function openTab(evt, tabName) {\n"
"            var i, tabcontent, tablinks;\n"
"            tabcontent = document.getElementsByClassName(\"tabcontent\");\n"
"            for (i = 0; i < tabcontent.length; i++) {\n"
"                tabcontent[i].style.display = \"none\";\n"
"            }\n"
"            tablinks = document.getElementsByClassName(\"tablinks\");\n"
"            for (i = 0; i < tablinks.length; i++) {\n"
"                tablinks[i].className = tablinks[i].className.replace(\" active\", \"\");\n"
"            }\n"
"            document.getElementById(tabName).style.display = \"block\";\n"
"            evt.currentTarget.className += \" active\";\n"
"        }\n"
"        \n"
"        // Update data more frequently (every 500ms)\n"
"        setInterval(updateAllData, 500);\n"
"        updateAllData();\n"
"        \n"
"        function updateAllData() {\n"
"            updateClimateData();\n"
"            loadAuthorizedCards();\n"
"        }\n"
"        \n"
"        function updateClimateData() {\n"
"            fetch('/data')\n"
"                .then(response => response.text())\n"
"                .then(data => {\n"
"                    const parts = data.split(',');\n"
"                    document.getElementById('temp').textContent = parts[0];\n"
"                    document.getElementById('hum').textContent = parts[1];\n"
"                    document.getElementById('climate-update-time').textContent = new Date().toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'});\n"
"                    \n"
"                    const fanStatus = parts[2] === '1';\n"
"                    const heaterStatus = parts[3] === '1';\n"
"                    \n"
"                    document.getElementById('fan').textContent = fanStatus ? 'ON' : 'OFF';\n"
"                    document.getElementById('heater').textContent = heaterStatus ? 'ON' : 'OFF';\n"
"                    \n"
"                    document.getElementById('fan-indicator').className = 'status-indicator ' + (fanStatus ? 'on' : 'off');\n"
"                    document.getElementById('heater-indicator').className = 'status-indicator ' + (heaterStatus ? 'on' : 'off');\n"
"                    \n"
"                    // RFID status\n"
"                    document.getElementById('last-uid').textContent = parts[4];\n"
"                    const rfidAuthorized = parts[5] === '1';\n"
"                    const statusElement = document.getElementById('rfid-status');\n"
"                    statusElement.textContent = rfidAuthorized ? 'AUTHORIZED' : 'DENIED';\n"
"                    statusElement.className = rfidAuthorized ? 'rfid-authorized' : 'rfid-denied';\n"
"                    \n"
"                    // NRF24L01 status\n"
"                    document.getElementById('nrf-mode').textContent = parts[6] === '1' ? 'Transmitter (TX)' : 'Receiver (RX)';\n"
"                    \n"
"                    // Handle NRF message (parts[7]) - Check if it's a new message\n"
"                    const nrfMessage = parts.slice(7).join(',');  // Rejoin in case message contains commas\n"
"                    if (nrfMessage && nrfMessage !== lastNrfMessage && nrfMessage !== 'None') {\n"
"                        lastNrfMessage = nrfMessage;\n"
"                        nrfUpdateTimestamp = new Date().toLocaleTimeString();\n"
"                        \n"
"                        // Update raw message display\n"
"                        document.getElementById('nrf-raw-message').textContent = nrfMessage;\n"
"                        document.getElementById('nrf-timestamp').textContent = nrfUpdateTimestamp;\n"
"                        \n"
"                        // Try to parse comma-separated values if present\n"
"                        const messageValues = nrfMessage.split(',');\n"
"                        if (messageValues.length > 1) {\n"
"                            document.getElementById('nrf-parsed-values').style.display = 'block';\n"
"                            document.getElementById('nrf-value-1').textContent = messageValues[0] || '--';\n"
"                            document.getElementById('nrf-value-2').textContent = messageValues[1] || '--';\n"
"                            document.getElementById('nrf-value-3').textContent = messageValues[2] || '--';\n"
"                        } else {\n"
"                            document.getElementById('nrf-parsed-values').style.display = 'none';\n"
"                        }\n"
"                    } else if (nrfMessage === 'None') {\n"
"                        document.getElementById('nrf-raw-message').textContent = 'No message received';\n"
"                        document.getElementById('nrf-parsed-values').style.display = 'none';\n"
"                    }\n"
"                })\n"
"                .catch(error => {\n"
"                    console.error('Error fetching climate data:', error);\n"
"                });\n"
"        }\n"
"        \n"
"        function loadAuthorizedCards() {\n"
"            fetch('/rfid_cards')\n"
"                .then(response => response.text())\n"
"                .then(data => {\n"
"                    document.getElementById('authorized-cards').innerHTML = data;\n"
"                    document.getElementById('rfid-update-time').textContent = new Date().toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'});\n"
"                })\n"
"                .catch(error => {\n"
"                    console.error('Error fetching RFID cards:', error);\n"
"                });\n"
"        }\n"
"        \n"
"        function control(device, action) {\n"
"            fetch(`/control?device=${device}&action=${action}`);\n"
"            setTimeout(updateClimateData, 300);\n"
"        }\n"
"        \n"
"        function doorControl(action) {\n"
"            fetch(`/door?action=${action}`)\n"
"                .then(response => response.text())\n"
"                .then(data => {\n"
"                    alert(data);\n"
"                });\n"
"        }\n"
"        \n"
"        function nrfControl(type, value) {\n"
"            fetch(`/nrf_control?type=${type}&value=${value}`)\n"
"                .then(response => response.text())\n"
"                .then(data => {\n"
"                    alert(data);\n"
"                    updateClimateData();\n"
"                });\n"
"        }\n"
"        \n"
"        function sendNRFMessage() {\n"
"            const message = document.getElementById('message-input').value;\n"
"            if (message) {\n"
"                fetch(`/nrf_send?message=${encodeURIComponent(message)}`)\n"
"                    .then(response => response.text())\n"
"                    .then(data => {\n"
"                        alert(data);\n"
"                        document.getElementById('message-input').value = '';\n"
"                    });\n"
"            } else {\n"
"                alert('Please enter a message to send');\n"
"            }\n"
"        }\n"
"        \n"
"        function sendQuickCommand(command) {\n"
"            fetch(`/nrf_send?message=${command}`)\n"
"                .then(response => response.text())\n"
"                .then(data => {\n"
"                    alert(data);\n"
"                });\n"
"        }\n"
"    </script>\n"
"</body>\n"
"</html>";

// Update RFID status for web interface
void update_rfid_status(bool authorized, const char* uid_str)
{
    rfid_status_updated = true;
    rfid_last_authorized = authorized;
    strcpy(rfid_last_uid, uid_str);
}

// Initialize WiFi as Access Point
void wifi_init_softap(void)
{
    ESP_LOGI(TAG, "Initializing WiFi in AP mode");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP adapter
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Configure AP settings
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = 1,
            .password = WIFI_AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    if (strlen(WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started with SSID: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "AP IP address: 192.168.4.1");
}

// HTTP Server Handlers
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

static esp_err_t data_handler(httpd_req_t *req)
{
    char resp[300];
    
    // Format response with all system data including NRF24L01
    // Format: temp,humidity,fan,heater,rfid_uid,rfid_auth,nrf_mode,nrf_message
    // Note: The NRF message may contain commas, so it should be the last element
    
    // First part with fixed format
    int written = snprintf(resp, sizeof(resp), "%d,%d,%d,%d,%s,%d,%d,", 
             latest_sensor_data.temperature,
             latest_sensor_data.humidity,
             fan_state ? 1 : 0,
             heater_state ? 1 : 0,
             rfid_last_uid,
             rfid_last_authorized ? 1 : 0,
             nrf_current_mode == NRF_MODE_TX ? 1 : 0);
    
    // Add the NRF message - make sure there's space
    if (written < sizeof(resp) && strlen(last_nrf_message) > 0) {
        strncat(resp + written, last_nrf_message, sizeof(resp) - written - 1);
    } else if (written < sizeof(resp)) {
        // No message or empty message
        strncat(resp + written, "None", sizeof(resp) - written - 1);
    }
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t control_handler(httpd_req_t *req)
{
    char buf[100] = {0};
    char device[10] = {0};
    char action[10] = {0};
    
    // Get query string
    int buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        httpd_req_get_url_query_str(req, buf, buf_len);
        
        // Parse device and action parameters
        if (httpd_query_key_value(buf, "device", device, sizeof(device)) == ESP_OK &&
            httpd_query_key_value(buf, "action", action, sizeof(action)) == ESP_OK) {
            
            ESP_LOGI(TAG, "Control request: device=%s, action=%s", device, action);
            
            if (strcmp(device, "fan") == 0) {
                if (strcmp(action, "auto") == 0) {
                    fan_override = false;
                } else if (strcmp(action, "on") == 0) {
                    fan_override = true;
                    set_fan_state(true);
                } else if (strcmp(action, "off") == 0) {
                    fan_override = true;
                    set_fan_state(false);
                }
            }
            else if (strcmp(device, "heater") == 0) {
                if (strcmp(action, "auto") == 0) {
                    heater_override = false;
                } else if (strcmp(action, "on") == 0) {
                    heater_override = true;
                    set_heater_state(true);
                } else if (strcmp(action, "off") == 0) {
                    heater_override = true;
                    set_heater_state(false);
                }
            }
        }
    }
    
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

static esp_err_t door_handler(httpd_req_t *req)
{
    char buf[100] = {0};
    char action[10] = {0};
    char resp[100] = {0};
    
    // Get query string
    int buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        httpd_req_get_url_query_str(req, buf, buf_len);
        
        // Parse action parameter
        if (httpd_query_key_value(buf, "action", action, sizeof(action)) == ESP_OK) {
            ESP_LOGI(TAG, "Door control request: action=%s", action);
            
            if (strcmp(action, "open") == 0) {
                // Call the door open function (same as used by RFID)
                servo_move(DOOR_OPEN_ANGLE, DOOR_SERVO_CHANNEL, DOOR_SERVO_MODE);
                strcpy(resp, "Door opened");
                
                // Start the timer to close the door after the configured time
                esp_timer_stop(door_close_timer);
                esp_timer_start_once(door_close_timer, DOOR_OPEN_TIME_MS * 1000);
            } 
            else if (strcmp(action, "close") == 0) {
                servo_move(DOOR_CLOSED_ANGLE, DOOR_SERVO_CHANNEL, DOOR_SERVO_MODE);
                strcpy(resp, "Door closed");
            }
            else {
                strcpy(resp, "Invalid door action");
            }
        }
        else {
            strcpy(resp, "Missing action parameter");
        }
    }
    else {
        strcpy(resp, "Missing query string");
    }
    
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t nrf_control_handler(httpd_req_t *req)
{
    char buf[100] = {0};
    char type[10] = {0};
    char value[32] = {0};
    char resp[100] = {0};
    
    // Get query string
    int buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        httpd_req_get_url_query_str(req, buf, buf_len);
        
        // Parse parameters
        if (httpd_query_key_value(buf, "type", type, sizeof(type)) == ESP_OK &&
            httpd_query_key_value(buf, "value", value, sizeof(value)) == ESP_OK) {
            
            ESP_LOGI(TAG, "NRF control request: type=%s, value=%s", type, value);
            
            if (strcmp(type, "mode") == 0) {
                if (strcmp(value, "rx") == 0) {
                    // Switch to receiver mode
                    if (nrf_current_mode != NRF_MODE_RX) {
                        initialize_nrf24l01(NRF_MODE_RX);
                        // Restart the NRF task (would need proper task management)
                        strcpy(resp, "Switched to RX (receiver) mode");
                    } else {
                        strcpy(resp, "Already in RX mode");
                    }
                } 
                else if (strcmp(value, "tx") == 0) {
                    // Switch to transmitter mode
                    if (nrf_current_mode != NRF_MODE_TX) {
                        initialize_nrf24l01(NRF_MODE_TX);
                        // Restart the NRF task (would need proper task management)
                        strcpy(resp, "Switched to TX (transmitter) mode");
                    } else {
                        strcpy(resp, "Already in TX mode");
                    }
                }
                else {
                    strcpy(resp, "Invalid mode value");
                }
            }
            else {
                strcpy(resp, "Invalid control type");
            }
        }
        else {
            strcpy(resp, "Missing parameters");
        }
    }
    else {
        strcpy(resp, "Missing query string");
    }
    
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t nrf_send_handler(httpd_req_t *req)
{
    char buf[100] = {0};
    char message[33] = {0};
    char resp[100] = {0};
    
    // Get query string
    int buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        httpd_req_get_url_query_str(req, buf, buf_len);
        
        // Parse message parameter
        if (httpd_query_key_value(buf, "message", message, sizeof(message)) == ESP_OK) {
            ESP_LOGI(TAG, "NRF send request: message=%s", message);
            
            if (nrf_current_mode == NRF_MODE_TX) {
                // Send the message
                nrf_send_data((uint8_t*)message, strlen(message));
                sprintf(resp, "Message sent: %s", message);
            } else {
                strcpy(resp, "Cannot send - device is in RX mode");
            }
        }
        else {
            strcpy(resp, "Missing message parameter");
        }
    }
    else {
        strcpy(resp, "Missing query string");
    }
    
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t rfid_cards_handler(httpd_req_t *req)
{
    esp_err_t err;
    nvs_handle_t my_handle;
    uint8_t stored_uids[MAX_UIDS][UID_LENGTH];
    size_t stored_uid_size = sizeof(stored_uids);
    size_t uid_count = 0;
    
    httpd_resp_set_type(req, "text/html");
    
    // Open NVS
    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        httpd_resp_send(req, "<p>Error accessing stored cards</p>", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }
    
    // Read stored UIDs
    err = nvs_get_blob(my_handle, "card_uids", stored_uids, &stored_uid_size);
    if (err == ESP_OK) {
        uid_count = stored_uid_size / sizeof(stored_uids[0]);
        
        if (uid_count == 0) {
            httpd_resp_send(req, "<p>No authorized cards stored</p>", HTTPD_RESP_USE_STRLEN);
            nvs_close(my_handle);
            return ESP_OK;
        }
        
        // Start table
        char *header = "<table><tr><th>Card #</th><th>UID</th></tr>";
        httpd_resp_send_chunk(req, header, strlen(header));
        
        // Add each card to the table
        char row[150];
        for (size_t i = 0; i < uid_count; i++) {
            // Generate UID string
            char uid_str[50] = "";
            int offset = 0;
            for (int j = 0; j < UID_LENGTH && stored_uids[i][j] != 0; j++) {
                offset += snprintf(uid_str + offset, sizeof(uid_str) - offset, 
                                  "%02X", stored_uids[i][j]);
                if (j < UID_LENGTH - 1 && stored_uids[i][j+1] != 0) {
                    offset += snprintf(uid_str + offset, sizeof(uid_str) - offset, ":");
                }
            }
            
            // Create table row
            snprintf(row, sizeof(row), 
                    "<tr><td>%d</td><td>%s</td></tr>", 
                    (int)(i + 1), uid_str);
            httpd_resp_send_chunk(req, row, strlen(row));
        }
        
        // End table
        httpd_resp_send_chunk(req, "</table>", HTTPD_RESP_USE_STRLEN);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        httpd_resp_send(req, "<p>No authorized cards stored</p>", HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send(req, "<p>Error reading stored cards</p>", HTTPD_RESP_USE_STRLEN);
    }
    
    nvs_close(my_handle);
    httpd_resp_send_chunk(req, NULL, 0); // End response
    return ESP_OK;
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root);
        
        httpd_uri_t data = {
            .uri = "/data",
            .method = HTTP_GET,
            .handler = data_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &data);
        
        httpd_uri_t control = {
            .uri = "/control",
            .method = HTTP_GET,
            .handler = control_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &control);
        
        httpd_uri_t rfid_cards = {
            .uri = "/rfid_cards",
            .method = HTTP_GET,
            .handler = rfid_cards_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &rfid_cards);
        
        // Register new NRF24L01 handlers
        httpd_uri_t door = {
            .uri = "/door",
            .method = HTTP_GET,
            .handler = door_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &door);
        
        httpd_uri_t nrf_control = {
            .uri = "/nrf_control",
            .method = HTTP_GET,
            .handler = nrf_control_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &nrf_control);
        
        httpd_uri_t nrf_send = {
            .uri = "/nrf_send",
            .method = HTTP_GET,
            .handler = nrf_send_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &nrf_send);
        
        ESP_LOGI(TAG, "Web server started");
    } else {
        ESP_LOGE(TAG, "Error starting server!");
    }
}

// Function to setup WiFi and start the webserver
void wifi_webserver_init(void)
{
    // Initialize WiFi
    wifi_init_softap();
    
    // Start webserver
    start_webserver();
    
    ESP_LOGI(TAG, "WiFi and web server initialized. Connect to %s", WIFI_AP_SSID);
}