
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


void toggleSpeedMode();
void singleStep();
void updateOLED(String status, String speed);
void handleRoot();
void handleCW();
void handleCCW();
void handleStop();
void handleToggleSpeed();

const char* ssid = "Prateek";     
const char* password = "jde@#12345";   

WebServer server(80);

// --- OLED Setup ---
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET     -1 
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


const int dirPin = 18;
const int stepPin = 19;
const int enablePin = 23;


const int btnCW = 14;     
const int btnCCW = 27;    
const int btnSpeed = 26;  

int stepDelay = 1000;    
bool isFastMode = false;
bool lastSpeedBtnState = HIGH;

String currentStatus = "STOPPED";
String lastStatus = "";
String currentSpeedStr = "SLOW";
String lastSpeedStr = "";
String ipAddressStr = "0.0.0.0";


const char HTML_PAGE[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Stepper Control Hub</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;700&display=swap" rel="stylesheet">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    
    <style>
        :root {
            --bg-grad: linear-gradient(135deg, #0f172a 0%, #1e1b4b 100%);
            --panel-bg: rgba(30, 41, 59, 0.7);
            --border-glow: rgba(255, 255, 255, 0.08);
            --accent-blue: #38bdf8;
            --accent-green: #10b981;
            --accent-red: #f43f5e;
            --accent-amber: #f59e0b;
            --text-light: #f8fafc;
            --text-muted: #94a3b8;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Inter', sans-serif; }
        
        body {
            background: var(--bg-grad);
            color: var(--text-light);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }

        .dashboard {
            width: 100%;
            max-width: 450px;
            background: var(--panel-bg);
            backdrop-filter: blur(12px);
            -webkit-backdrop-filter: blur(12px);
            border: 1px solid var(--border-glow);
            border-radius: 24px;
            padding: 30px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.4);
        }

        header { text-align: center; margin-bottom: 25px; }
        header h1 { font-size: 22px; font-weight: 700; letter-spacing: -0.5px; margin-bottom: 5px; }
        header p { color: var(--text-muted); font-size: 13px; }

        .status-container {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
            margin-bottom: 25px;
        }
        .tile {
            background: rgba(15, 23, 42, 0.4);
            border: 1px solid var(--border-glow);
            border-radius: 14px;
            padding: 12px 16px;
            display: flex;
            align-items: center;
            gap: 12px;
        }
        .tile i { font-size: 20px; color: var(--accent-blue); }
        .tile-data { display: flex; flex-direction: column; }
        .tile-label { font-size: 11px; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.5px; }
        .tile-value { font-size: 15px; font-weight: 600; }

        .control-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-bottom: 15px;
        }

        .btn {
            background: rgba(255,255,255,0.05);
            border: 1px solid rgba(255,255,255,0.1);
            color: var(--text-light);
            padding: 16px;
            font-size: 15px;
            font-weight: 600;
            border-radius: 14px;
            cursor: pointer;
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 8px;
            transition: all 0.2s ease;
        }
        .btn i { font-size: 18px; }
        .btn:hover { background: rgba(255,255,255,0.1); transform: translateY(-2px); }
        .btn:active { transform: translateY(0); }

        .btn-cw:hover { border-color: var(--accent-green); color: var(--accent-green); box-shadow: 0 0 15px rgba(16,185,129,0.2); }
        .btn-ccw:hover { border-color: var(--accent-blue); color: var(--accent-blue); box-shadow: 0 0 15px rgba(56,189,248,0.2); }
        
        .btn-full { grid-column: span 2; flex-direction: row; justify-content: center; gap: 12px; }
        
        .btn-stop { background: rgba(244, 63, 94, 0.1); border-color: rgba(244, 63, 94, 0.3); color: #f43f5e; }
        .btn-stop:hover { background: var(--accent-red); color: white; border-color: var(--accent-red); box-shadow: 0 0 20px rgba(244,63,94,0.4); }
        
        .btn-speed { border-color: rgba(245, 158, 11, 0.3); color: var(--accent-amber); }
        .btn-speed:hover { background: rgba(245, 158, 11, 0.1); }
    </style>

    <script>
        function sendAction(action, displayStatus, displaySpeed) {
            fetch('/' + action);
            if(displayStatus) document.getElementById('webStatus').innerText = displayStatus;
            if(displaySpeed) {
                let speedElem = document.getElementById('webSpeed');
                if(speedElem.innerText === "SLOW") speedElem.innerText = "FAST";
                else speedElem.innerText = "SLOW";
            }
        }
    </script>
</head>
<body>

<div class="dashboard">
    <header>
        <h1>System Control Panel</h1>
        <p>Wireless Stepper Engine Interface</p>
    </header>

    <div class="status-container">
        <div class="tile">
            <i class="fa-solid fa-circle-notch fa-spin" style="--fa-animation-duration: 3s;"></i>
            <div class="tile-data">
                <span class="tile-label">Engine Status</span>
                <span class="tile-value" id="webStatus">READY</span>
            </div>
        </div>
        <div class="tile">
            <i class="fa-solid fa-gauge-high"></i>
            <div class="tile-data">
                <span class="tile-label">Velocity Mode</span>
                <span class="tile-value" id="webSpeed">SLOW</span>
            </div>
        </div>
    </div>

    <div class="control-grid">
        <button class="btn btn-cw" onclick="sendAction('cw', 'RUNNING CW', null)">
            <i class="fa-solid fa-rotate-right"></i>
            Forward (CW)
        </button>
        <button class="btn btn-ccw" onclick="sendAction('ccw', 'RUNNING CCW', null)">
            <i class="fa-solid fa-rotate-left"></i>
            Reverse (CCW)
        </button>
        <button class="btn btn-full btn-stop" onclick="sendAction('stop', 'STOPPED', null)">
            <i class="fa-solid fa-shield-halved"></i>
            Emergency Stop
        </button>
        <button class="btn btn-full btn-speed" onclick="sendAction('toggle_speed', null, true)">
            <i class="fa-solid fa-bolt"></i>
            Shift Velocity Gear
        </button>
    </div>
</div>

</body>
</html>
)=====";


void setup() {
  Serial.begin(115200);

  // Initialize Motor Pins
  pinMode(stepPin, OUTPUT);
  pinMode(dirPin, OUTPUT);
  pinMode(enablePin, OUTPUT);
  digitalWrite(enablePin, LOW); 


  pinMode(btnCW, INPUT_PULLUP);
  pinMode(btnCCW, INPUT_PULLUP);
  pinMode(btnSpeed, INPUT_PULLUP);


  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.print("Connecting to WiFi...");
  display.display();

 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  ipAddressStr = WiFi.localIP().toString();
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(ipAddressStr);


  server.on("/", handleRoot);
  server.on("/cw", handleCW);
  server.on("/ccw", handleCCW);
  server.on("/stop", handleStop);
  server.on("/toggle_speed", handleToggleSpeed);
  server.begin();

  updateOLED("READY", "SLOW");
}

void loop() {
 
  server.handleClient();


  bool cwPressed = digitalRead(btnCW) == LOW;
  bool ccwPressed = digitalRead(btnCCW) == LOW;
  bool speedPressed = digitalRead(btnSpeed) == LOW;

 
  if (speedPressed && lastSpeedBtnState == HIGH) {
    toggleSpeedMode();
    delay(50); 
  }
  lastSpeedBtnState = speedPressed;


  if (cwPressed && !ccwPressed) {
    currentStatus = "RUNNING CW";
  } 
  else if (ccwPressed && !cwPressed) {
    currentStatus = "RUNNING CCW";
  } 
  else {
    
    static bool lastFramePhysicalPressed = false;
    bool currentFramePhysicalPressed = cwPressed || ccwPressed;
    
    if (!currentFramePhysicalPressed && lastFramePhysicalPressed) {
      currentStatus = "STOPPED";
    }
    lastFramePhysicalPressed = currentFramePhysicalPressed;
  }


  if (currentStatus == "RUNNING CW") {
    digitalWrite(dirPin, HIGH);
    singleStep();
  } 
  else if (currentStatus == "RUNNING CCW") {
    digitalWrite(dirPin, LOW);
    singleStep();
  }

 
  if (currentStatus != lastStatus || currentSpeedStr != lastSpeedStr) {
    updateOLED(currentStatus, currentSpeedStr);
    lastStatus = currentStatus;
    lastSpeedStr = currentSpeedStr;
  }
}


void singleStep() {
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(stepDelay);
  digitalWrite(stepPin, LOW);
  delayMicroseconds(stepDelay);
}

void toggleSpeedMode() {
  isFastMode = !isFastMode;
  if (isFastMode) {
    stepDelay = 400;  
    currentSpeedStr = "FAST";
  } else {
    stepDelay = 1200; 
    currentSpeedStr = "SLOW";
  }
}

void updateOLED(String status, String speed) {
  display.clearDisplay();
  

  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  
  display.fillRect(1, 1, 126, 14, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(15, 4);
  display.print("STEPPER NET-PANEL");
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 22);
  display.print("STATUS: "); display.print(status);
  
  display.setCursor(8, 36);
  display.print("SPEED : "); display.print(speed);
  
  display.setCursor(8, 51);
  display.print("IP: "); display.print(ipAddressStr);
  
  display.display();
}

void handleRoot() {
  server.send_P(200, "text/html", HTML_PAGE);
}

void handleCW() {
  currentStatus = "RUNNING CW";
  server.send(200, "text/plain", "OK");
}

void handleCCW() {
  currentStatus = "RUNNING CCW";
  server.send(200, "text/plain", "OK");
}

void handleStop() {
  currentStatus = "STOPPED";
  server.send(200, "text/plain", "OK");
}

void handleToggleSpeed() {
  toggleSpeedMode();
  server.send(200, "text/plain", "OK");
}