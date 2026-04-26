#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "time.h"

const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";
const char* googleMapsApiKey = "YOUR_GOOGLE_MAPS_API_KEY";

const String binID = "1";
const char* otherBinIP = "http://192.168.1.102";

float binLat = 12.9716, binLng = 77.5946;
float otherBinLat = 12.2958, otherBinLng = 76.6394;

#define TRIG_PIN 14  
#define ECHO_PIN 32  

String otherBinStatus = "Unknown";
bool binFullAlert = false;
String lastCollectionTime;

Preferences preferences;
WebServer server(80);

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

String getCurrentTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "Time not available";
    
    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeString);
}

bool isBinFull() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH);
    float distance = (duration * 0.0343) / 2;  
    return (distance <= 10);
}

void checkOtherBin() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(String(otherBinIP) + "/status");
        int httpResponseCode = http.GET();
        if (httpResponseCode == 200) {
            otherBinStatus = http.getString();
        } else {
            otherBinStatus = "Unknown";
        }
        http.end();
    }
}

String generateHTML() {
    String html = "<html><head><meta http-equiv='refresh' content='5'>";
    html += "<script src='https://maps.googleapis.com/maps/api/js?key=" + String(googleMapsApiKey) + "'></script>";
    html += "<script>function initMap(){";
    html += "var bin1={lat:" + String(binLat) + ",lng:" + String(binLng) + "};";
    html += "var bin2={lat:" + String(otherBinLat) + ",lng:" + String(otherBinLng) + "};";
    html += "var map=new google.maps.Map(document.getElementById('map'),{zoom:14,center:bin1});";
    html += "var marker1=new google.maps.Marker({position:bin1,map:map,label:'Bin " + binID + "',";
    html += "icon:{url:'http://maps.google.com/mapfiles/ms/icons/" + String(binFullAlert ? "red-dot.png" : "green-dot.png") + "'}});";
    html += "var marker2=new google.maps.Marker({position:bin2,map:map,label:'Bin " + String(binID == "1" ? "2" : "1") + "',";
    html += "icon:{url:'http://maps.google.com/mapfiles/ms/icons/" + String(otherBinStatus == "Full" ? "red-dot.png" : "green-dot.png") + "'}});";
    html += "} window.onload=initMap;</script>";
    html += "</head><body><h1>Smart Bin Monitoring - Bin " + binID + "</h1>";

    if (binFullAlert) {
        html += "<h2 style='color:red;'>⚠️ Bin " + binID + " is full! Please use another bin. ⚠️</h2>";
    }
    if (binFullAlert && otherBinStatus == "Available") {
        html += "<h2 style='color:blue;'>➡️ Bin " + binID + " is full. Please use Bin " + String(binID == "1" ? "2" : "1") + ".</h2>";
        html += "<h3>Nearest Bin Location:</h3>";
        html += "<iframe width='600' height='400' frameborder='0' style='border:0'";
        html += "src='https://www.google.com/maps/embed/v1/place?key=" + String(googleMapsApiKey);
        html += "&q=" + String(otherBinLat) + "," + String(otherBinLng) + "' allowfullscreen></iframe>";
    }

    html += "<p><b>Last Collection Time:</b> " + lastCollectionTime + "</p>";
    html += "<p><b>Other Bin Status:</b> " + otherBinStatus + "</p>";
    html += "<h2>Bin Locations</h2><div id='map' style='height:400px;width:100%;'></div></body></html>";
    return html;
}

void handleStatus() {
    server.send(200, "text/plain", binFullAlert ? "Full" : "Available");
}

void handleRoot() {
    server.send(200, "text/html", generateHTML());
}

void setup() {
    Serial.begin(115200);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    preferences.begin("binData", false);
    lastCollectionTime = preferences.getString("lastCollected", "Not collected yet");
    preferences.end();

    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.begin();
}

void loop() {
    server.handleClient();
    bool currentBinStatus = isBinFull();
    
    if (!currentBinStatus && binFullAlert) {
        lastCollectionTime = getCurrentTime();
        preferences.begin("binData", false);
        preferences.putString("lastCollected", lastCollectionTime);
        preferences.end();
    }

    binFullAlert = currentBinStatus;
    checkOtherBin();
    delay(5000);
}