#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// --- Configuration ---
const char* ssid = "SpiderRobot_Pro";
const char* password = "12345678";

WebServer server(80);
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

#define SERVO_FREQ 50
#define SERVO_MIN 125 
#define SERVO_MAX 500 

// --- State Management ---
enum RobotState { IDLE, STAND, SIT, FORWARD, BACKWARD, LEFT, RIGHT, DANCE, WIGGLE, ATTACK };
RobotState currentState = STAND;

const int numServos = 12;
float currentAngle[numServos];
float targetAngle[numServos];

unsigned long lastServoUpdate = 0;
unsigned long lastSequenceUpdate = 0;
int sequenceStep = 0;
float servoStepSize = 2.0;

// --- Clean & Modern UI ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Spider Hub</title>
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <style>
        :root { --primary: #4776E6; --secondary: #8E54E9; --bg: #f0f2f5; --card: #ffffff; --text: #2d3436; }
        body { font-family: 'Segoe UI', system-ui, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
        .app-card { background: var(--card); padding: 25px; border-radius: 24px; box-shadow: 0 10px 30px rgba(0,0,0,0.08); width: 100%; max-width: 400px; }
        h1 { font-size: 24px; font-weight: 700; margin-bottom: 5px; background: linear-gradient(to right, var(--primary), var(--secondary)); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
        #status { font-size: 14px; color: #636e72; margin-bottom: 25px; text-transform: uppercase; letter-spacing: 1px; }
        
        /* D-PAD Style */
        .d-pad { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; margin-bottom: 30px; }
        .btn { border: none; background: #f8f9fa; color: var(--text); padding: 18px; border-radius: 16px; font-weight: 600; cursor: pointer; transition: 0.2s; box-shadow: 0 4px 6px rgba(0,0,0,0.02); }
        .btn:active { transform: scale(0.95); background: #e9ecef; }
        .btn-main { background: linear-gradient(135deg, var(--primary), var(--secondary)); color: white; box-shadow: 0 4px 15px rgba(71, 118, 230, 0.3); }
        
        /* Action Grid */
        .action-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
        .btn-action { background: #ffffff; border: 1px solid #e0e0e0; font-size: 14px; }
        .btn-stop { grid-column: span 2; background: #ff7675; color: white; margin-top: 10px; border: none; }
    </style>
</head>
<body>
    <div class="app-card">
        <h1>Spider Control</h1>
        <div id="status">System: Ready</div>
        
        <div class="d-pad">
            <div></div><button class="btn" onclick="send('forward')">▲</button><div></div>
            <button class="btn" onclick="send('left')">◀</button>
            <button class="btn btn-main" onclick="send('stand')">●</button>
            <button class="btn" onclick="send('right')">▶</button>
            <div></div><button class="btn" onclick="send('backward')">▼</button><div></div>
        </div>

        <div class="action-grid">
            <button class="btn btn-action" onclick="send('dance')">Dance</button>
            <button class="btn btn-action" onclick="send('wiggle')">Wiggle</button>
            <button class="btn btn-action" onclick="send('sit')">Sit Down</button>
            <button class="btn btn-action" onclick="send('attack')">Attack</button>
            <button class="btn btn-stop" onclick="send('stop')">Stop All</button>
        </div>
    </div>

    <script>
        function send(cmd) {
            fetch(`/action?cmd=${cmd}`);
            document.getElementById('status').innerText = "Running: " + cmd;
        }
    </script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    pwm.begin();
    pwm.setPWMFreq(SERVO_FREQ);

    for(int i = 0; i < numServos; i++) {
        currentAngle[i] = 90.0;
        targetAngle[i] = 90.0;
        pwm.setPWM(i, 0, map(90, 0, 180, SERVO_MIN, SERVO_MAX));
    }

    WiFi.softAP(ssid, password);
    server.on("/", []() { server.send(200, "text/html", index_html); });
    server.on("/action", handleAction);
    server.begin();
}

void loop() {
    server.handleClient();
    updateServoPositions();
    executeSequences();
}

void updateServoPositions() {
    if (millis() - lastServoUpdate >= 15) {
        for(int i = 0; i < numServos; i++) {
            if (abs(currentAngle[i] - targetAngle[i]) > 0.5) {
                currentAngle[i] += (targetAngle[i] > currentAngle[i]) ? servoStepSize : -servoStepSize;
                pwm.setPWM(i, 0, map(currentAngle[i], 0, 180, SERVO_MIN, SERVO_MAX));
            }
        }
        lastServoUpdate = millis();
    }
}

void executeSequences() {
    if (currentState == IDLE) return;

    // --- Walking Gait Logic (Simplified Tripod) ---
    if (currentState == FORWARD || currentState == BACKWARD || currentState == LEFT || currentState == RIGHT) {
        if (millis() - lastSequenceUpdate > 350) {
            sequenceStep = (sequenceStep + 1) % 2;
            int moveVal = (currentState == FORWARD) ? 30 : (currentState == BACKWARD ? -30 : 0);
            
            for (int i = 0; i < numServos; i += 2) {
                bool isGroupA = (i == 0 || i == 4 || i == 8);
                if ((sequenceStep == 0 && isGroupA) || (sequenceStep == 1 && !isGroupA)) {
                    targetAngle[i] = 90 + moveVal;   // Hip Move
                    targetAngle[i+1] = 130;         // Leg Lift
                } else {
                    targetAngle[i] = 90 - moveVal;   // Hip Return
                    targetAngle[i+1] = 70;          // Leg Plant
                }
            }
            lastSequenceUpdate = millis();
        }
    }

    // --- Dance/Wiggle/Attack Logic ---
    else if (currentState == DANCE) {
        if (millis() - lastSequenceUpdate > 400) {
            sequenceStep = (sequenceStep + 1) % 4;
            for(int i = 0; i < numServos; i++) {
                if (sequenceStep == 0) targetAngle[i] = (i % 2 == 0) ? 60 : 120;
                else if (sequenceStep == 1) targetAngle[i] = (i % 2 == 0) ? 120 : 60;
                else if (sequenceStep == 2) targetAngle[i] = 90;
                else if (sequenceStep == 3) targetAngle[i] = (i < 6) ? 150 : 30;
            }
            lastSequenceUpdate = millis();
        }
    }
    
    else if (currentState == ATTACK) {
        targetAngle[0] = 160; targetAngle[1] = 20; // Raise Front Left
        targetAngle[3] = 160; targetAngle[4] = 20; // Raise Front Right
    }

    else if (currentState == STAND) {
        for(int i = 0; i < numServos; i++) targetAngle[i] = 90;
        currentState = IDLE;
    }
    
    else if (currentState == SIT) {
        for(int i = 0; i < numServos; i++) {
            if (i % 3 == 1) targetAngle[i] = 160; 
            else if (i % 3 == 2) targetAngle[i] = 20;
        }
        currentState = IDLE;
    }
}

void handleAction() {
    String cmd = server.arg("cmd");
    servoStepSize = 2.5; // Standard speed
    
    if (cmd == "forward") currentState = FORWARD;
    else if (cmd == "backward") currentState = BACKWARD;
    else if (cmd == "left") currentState = LEFT;
    else if (cmd == "right") currentState = RIGHT;
    else if (cmd == "dance") { currentState = DANCE; servoStepSize = 4.0; }
    else if (cmd == "wiggle") currentState = WIGGLE;
    else if (cmd == "attack") currentState = ATTACK;
    else if (cmd == "stand") currentState = STAND;
    else if (cmd == "sit") currentState = SIT;
    else if (cmd == "stop") {
        currentState = IDLE;
        for(int i=0; i<numServos; i++) targetAngle[i] = currentAngle[i]; 
    }
    server.send(200, "text/plain", "OK");
}
