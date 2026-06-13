#include <WiFi.h>
#include <WebServer.h>

// ========== WiFi Credentials ==========
// !!! IMPORTANT: Replace with your own network credentials !!!
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ========== Web Server ==========
WebServer server(80);

// ========== Audio Data ==========
// IMPORTANT: Replace this with your actual converted audio data array.
// You can use a tool like "XXD" or online WAV to C array converters.
// Example for a tiny beep sound (replace this entire block):
const uint8_t audio_data[] = {
  128, 128, 128, 130, 135, 140, 145, 150, 155, 160, 165, 170, 175, 180, 185, 190,
  195, 200, 200, 195, 190, 185, 180, 175, 170, 165, 160, 155, 150, 145, 140, 135,
  130, 128, 128, 128, 125, 120, 115, 110, 105, 100, 95, 90, 85, 80, 80, 85, 90, 95,
  100, 105, 110, 115, 120, 125, 128, 128, 128
};
const int audio_length = sizeof(audio_data) / sizeof(audio_data[0]);

// ========== Audio Control Variables ==========
int volume = 200;       // Volume range: 0-255 (255 = max)
int bass_level = 30;    // Bass level range: 0-100 (0 = no boost, 100 = max boost)
bool isPlaying = false; // Playback state

// For bass boost filter
float previous_sample = 128.0; // Initialize to middle value (for 8-bit audio 0-255)

// ========== Web Interface HTML ==========
void handleRoot() {
  String html = "<!DOCTYPE html><html>";
  html += "<head><title>ESP32 Audio Player</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; padding: 20px; background: #1a1a2e; color: white; }";
  html += "h2 { color: #ffcc00; text-shadow: 2px 2px 4px #000000; }";
  html += ".container { margin-top: 30px; }";
  html += "button { padding: 12px 30px; font-size: 18px; margin: 10px; border: none; background: #ffcc00; color: #1a1a2e; border-radius: 8px; cursor: pointer; transition: 0.3s; }";
  html += "button:hover { background: #ff9900; transform: scale(1.05); }";
  html += "button:active { transform: scale(0.95); }";
  html += "input[type='range'] { width: 80%; max-width: 300px; margin: 15px; -webkit-appearance: none; background: #333; height: 8px; border-radius: 5px; }";
  html += "input[type='range']::-webkit-slider-thumb { -webkit-appearance: none; width: 20px; height: 20px; background: #ffcc00; border-radius: 50%; cursor: pointer; }";
  html += "label { font-size: 18px; font-weight: bold; }";
  html += "p { font-size: 16px; margin: 5px; }";
  html += ".card { background: #16213e; padding: 20px; border-radius: 15px; max-width: 400px; margin: auto; box-shadow: 0 8px 16px rgba(0,0,0,0.3); }";
  html += "</style></head><body>";
  
  html += "<div class='card'>";
  html += "<h2>🎵 ESP32 Audio Player</h2>";
  html += "<div class='container'>";
  html += "<button onclick='playAudio()'>▶ Play</button>";
  html += "<button onclick='pauseAudio()'>⏸ Pause</button><br>";
  
  html += "<label>🔊 Volume: </label>";
  html += "<input type='range' id='volume' min='0' max='255' value='" + String(volume) + "' onchange='setVolume(this.value)'>";
  html += "<p>Volume: <span id='vol_val'>" + String(volume) + "</span></p>";
  
  html += "<label>🎸 Bass Boost: </label>";
  html += "<input type='range' id='bass' min='0' max='100' value='" + String(bass_level) + "' onchange='setBass(this.value)'>";
  html += "<p>Bass Level: <span id='bass_val'>" + String(bass_level) + "</span>%</p>";
  html += "</div></div>";
  
  html += "<script>";
  html += "function setVolume(vol) {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/setVolume?vol=' + vol, true);";
  html += "  xhr.send();";
  html += "  document.getElementById('vol_val').innerText = vol;";
  html += "}";
  html += "function setBass(bass) {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/setBass?bass=' + bass, true);";
  html += "  xhr.send();";
  html += "  document.getElementById('bass_val').innerText = bass;";
  html += "}";
  html += "function playAudio() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/play', true);";
  html += "  xhr.send();";
  html += "}";
  html += "function pauseAudio() {";
  html += "  var xhr = new XMLHttpRequest();";
  html += "  xhr.open('GET', '/pause', true);";
  html += "  xhr.send();";
  html += "}";
  html += "</script></body></html>";
  
  server.send(200, "text/html", html);
}

// ========== Control Handlers ==========
void handleSetVolume() {
  if (server.hasArg("vol")) {
    volume = server.arg("vol").toInt();
    volume = constrain(volume, 0, 255);
    Serial.println("Volume set to: " + String(volume));
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handleSetBass() {
  if (server.hasArg("bass")) {
    bass_level = server.arg("bass").toInt();
    bass_level = constrain(bass_level, 0, 100);
    Serial.println("Bass level set to: " + String(bass_level));
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad Request");
  }
}

void handlePlay() {
  isPlaying = true;
  Serial.println("Playback started");
  server.send(200, "text/plain", "Playing");
}

void handlePause() {
  isPlaying = false;
  Serial.println("Playback paused");
  server.send(200, "text/plain", "Paused");
}

// ========== Audio Processing Functions ==========
// Apply bass boost using a simple low-pass filter
uint8_t applyBassBoost(uint8_t sample) {
  float alpha = bass_level / 100.0;  // 0 = no filter, 1 = max filtering
  float current_sample = (float)sample;
  
  // Low-pass filter: y[n] = alpha * x[n] + (1-alpha) * y[n-1]
  float filtered_sample = alpha * current_sample + (1 - alpha) * previous_sample;
  previous_sample = filtered_sample;
  
  return constrain((int)filtered_sample, 0, 255);
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi");
  Serial.print("🌐 IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/setVolume", handleSetVolume);
  server.on("/setBass", handleSetBass);
  server.on("/play", handlePlay);
  server.on("/pause", handlePause);
  
  server.begin();
  Serial.println("🚀 Web server started");
}

// ========== Main Loop ==========
void loop() {
  server.handleClient();
  
  if (isPlaying) {
    for (int i = 0; i < audio_length && isPlaying; i++) {
      // Apply volume scaling
      uint8_t adjusted_sample = (audio_data[i] * volume) / 255;
      
      // Apply bass boost
      uint8_t final_sample = applyBassBoost(adjusted_sample);
      
      // Output to DAC on GPIO 25
      dacWrite(25, final_sample);
      
      // Delay for sample rate (~27.4 kHz for 36.5 microseconds)
      // For 8 kHz audio, use delayMicroseconds(125);
      delayMicroseconds(36.5);  // Adjust based on your audio sample rate
    }
    
    // If loop finishes naturally (end of audio), stop playback
    if (isPlaying) {
      isPlaying = false;
      Serial.println("Playback finished");
    }
  }
}