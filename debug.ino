#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// --- Configuration ---
const char* ssid = "SpiderRobot";
const char* password = "12345678";
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// --- Servo Settings ---
#define SERVO_FREQ 50
// Pulse lengths for 0 and 180 degrees (Tune these based on your specific SG90s)
#define SERVO_MIN 125 // ~0 degrees
#define SERVO_MAX 500 // ~180 degrees

// --- Motion Control Variables ---
const int numServos = 12;
float currentAngle[numServos];
float targetAngle[numServos];

unsigned long lastServoUpdate = 0;
const int servoInterval = 10;    // Update servos every 10ms
const float servoStepSize = 1.5; // Degrees to move per interval (adjust for speed)

// --- State Machine & Auto Sweep Variables ---
enum RobotState { IDLE, STAND, SIT, FORWARD, BACKWARD, LEFT, RIGHT, SWEEP };
RobotState currentState = STAND;

unsigned long lastSequenceUpdate = 0;
int sequenceStep = 0;
bool sweepIncreasing = true;

// --- HTML Interface ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SpiderRobot Control</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #121212; color: #ffffff; margin: 0; padding: 20px; text-align: center; }
        h1 { color: #00bcd4; margin-bottom: 20px; }
        .control-panel { display: flex; flex-wrap: wrap; justify-content: center; gap: 15px; margin-bottom: 30px; }
        .btn { background: #333; color: #fff; border: 1px solid #00bcd4; padding: 15px 25px; font-size: 16px; cursor: pointer; border-radius: 5px; transition: 0.3s; text-transform: uppercase; }
        .btn:hover { background: #00bcd4; color: #000; }
        .btn-stop { background: #d32f2f; border-color: #f44336; }
        .btn-stop:hover { background: #f44336; color: #fff; }
        .slider-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; max-width: 1200px; margin: auto; }
        .slider-card { background: #1e1e1e; padding: 15px; border-radius: 8px; border-left: 4px solid #00bcd4; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
        .slider-card span { display: block; margin-bottom: 10px; font-weight: bold; }
        input[type=range] { width: 100%; cursor: pointer; }
        #status { margin-bottom: 20px; font-size: 18px; color: #4caf50; }
    </style>
</head>
<body>
    <h1>SpiderRobot Controller</h1>
    <div id="status">Status: Stand</div>
    
    <div class="control-panel">
        <button class="btn" onclick="sendCommand('stand')">Stand</button>
        <button class="btn" onclick="sendCommand('sit')">Sit</button>
        <button class="btn" onclick="sendCommand('forward')">Forward</button>
        <button class="btn" onclick="sendCommand('backward')">Backward</button>
        <button class="btn" onclick="sendCommand('left')">Left</button>
        <button class="btn" onclick="sendCommand('right')">Right</button>
        <button class="btn" onclick="sendCommand('sweep_on')">Auto Sweep ON</button>
        <button class="btn" onclick="sendCommand('sweep_off')">Auto Sweep OFF</button>
        <button class="btn btn-stop" onclick="sendCommand('stop')">Stop</button>
    </div>

    <div class="slider-grid">
        <script>
            for(let i = 0; i < 12; i++) {
                document.write(`
                    <div class="slider-card">
                        <span>Servo ${i + 1} (<span id="val${i}">90</span>&deg;)</span>
                        <input type="range" min="0" max="180" value="90" id="s${i}" oninput="updateServo(${i}, this.value)">
                    </div>
                `);
            }
        </script>
    </div>

    <script>
        function updateServo(id, val) {
            document.getElementById('val' + id).innerText = val;
            fetch(`/set?servo=${id}&angle=${val}`);
            document.getElementById('status').innerText = `Status: Manual Override (Servo ${id + 1})`;
        }
        function sendCommand(cmd) {
            fetch(`/action?cmd=${cmd}`);
            document.getElementById('status').innerText = `Status: ${cmd.toUpperCase()}`;
            // Reset sliders to 90 visually on pose reset
            if(cmd === 'stand' || cmd === 'sit') {
                for(let i=0; i<12; i++) {
                    let val = (cmd === 'stand') ? 90 : (i%3===1 ? 150 : (i%3===2 ? 30 : 90));
                    document.getElementById('s'+i).value = val;
                    document.getElementById('val'+i).innerText = val;
                }
            }
        }
    </script>
</body>
</html>
)rawliteral";

// --- Function Prototypes ---
int angleToPulse(float angle);
void setServo(int channel, float angle);
void smoothMove(int channel, float target);
void handleRoot();
void handleSetServo();
void handleAction();
void updateServos();
void executeStateMachine();

// --- Setup ---
void setup() {
    Serial.begin(115200);
    
    // I2C Setup for PCA9685
    Wire.begin(21, 22); 
    pwm.begin();
    pwm.setOscillatorFrequency(27000000);
    pwm.setPWMFreq(SERVO_FREQ);

    // Initialize Arrays
    for(int i = 0; i < numServos; i++) {
        currentAngle[i] = 90.0;
        targetAngle[i] = 90.0;
        setServo(i, 90.0); // Jump to 90 initially
    }

    // WiFi Setup (Access Point)
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP(ssid, password);
    Serial.println("AP Setup Complete.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());

    // Web Server Routing
    server.on("/", HTTP_GET, handleRoot);
    server.on("/set", HTTP_GET, handleSetServo);
    server.on("/action", HTTP_GET, handleAction);
    server.begin();
}

// --- Main Loop ---
void loop() {
    server.handleClient();
    updateServos();
    executeStateMachine();
}

// --- Core Servo Functions ---

// Converts 0-180 degree angle to PCA9685 pulse width
int angleToPulse(float angle) {
    angle = constrain(angle, 0.0, 180.0);
    return map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
}

// Instantly jumps servo to angle (used for initialization)
void setServo(int channel, float angle) {
    pwm.setPWM(channel, 0, angleToPulse(angle));
    currentAngle[channel] = angle;
    targetAngle[channel] = angle;
}

// Sets the target angle for the non-blocking interpolation loop
void smoothMove(int channel, float target) {
    targetAngle[channel] = constrain(target, 0.0, 180.0);
}

// Interpolates current angle towards target angle smoothly
void updateServos() {
    if (millis() - lastServoUpdate >= servoInterval) {
        for(int i = 0; i < numServos; i++) {
            if (abs(currentAngle[i] - targetAngle[i]) > servoStepSize) {
                if (currentAngle[i] < targetAngle[i]) {
                    currentAngle[i] += servoStepSize;
                } else {
                    currentAngle[i] -= servoStepSize;
                }
                pwm.setPWM(i, 0, angleToPulse(currentAngle[i]));
            } else if (currentAngle[i] != targetAngle[i]) {
                // Snap to exact final target
                currentAngle[i] = targetAngle[i];
                pwm.setPWM(i, 0, angleToPulse(currentAngle[i]));
            }
        }
        lastServoUpdate = millis();
    }
}

// --- Sequence State Machine ---
void executeStateMachine() {
    if (currentState == IDLE) return;

    if (currentState == SWEEP) {
        if (millis() - lastSequenceUpdate > 1000) { // Reverse direction every 1 sec
            sweepIncreasing = !sweepIncreasing;
            float newTarget = sweepIncreasing ? 150.0 : 30.0;
            for(int i = 0; i < numServos; i++) {
                smoothMove(i, newTarget);
            }
            lastSequenceUpdate = millis();
        }
    } 
    else if (currentState == FORWARD) {
        // Placeholder for a non-blocking forward gait
        // You will replace the angles here based on your leg kinematics
        if (millis() - lastSequenceUpdate > 500) {
            sequenceStep = (sequenceStep + 1) % 2;
            if (sequenceStep == 0) {
                smoothMove(0, 120); smoothMove(3, 60); // Example front leg lift
            } else {
                smoothMove(0, 90); smoothMove(3, 90);  // Example front leg place
            }
            lastSequenceUpdate = millis();
        }
    }
    else if (currentState == STAND) {
        for(int i = 0; i < numServos; i++) smoothMove(i, 90.0);
        currentState = IDLE; // Execute once and stop
    }
    else if (currentState == SIT) {
        for(int i = 0; i < numServos; i++) {
            if (i % 3 == 0) smoothMove(i, 90.0);       // Coxa (Hip)
            else if (i % 3 == 1) smoothMove(i, 150.0); // Femur (Leg up)
            else smoothMove(i, 30.0);                  // Tibia (Foot tuck)
        }
        currentState = IDLE;
    }
    // Implement LEFT, RIGHT, BACKWARD similarly...
}

// --- Web Server Handlers ---
void handleRoot() {
    server.send(200, "text/html", index_html);
}

void handleSetServo() {
    if (server.hasArg("servo") && server.hasArg("angle")) {
        int servo = server.arg("servo").toInt();
        float angle = server.arg("angle").toFloat();
        if (servo >= 0 && servo < numServos) {
            currentState = IDLE; // Stop active patterns to allow manual control
            smoothMove(servo, angle);
        }
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Bad Request");
    }
}

void handleAction() {
    if (server.hasArg("cmd")) {
        String cmd = server.arg("cmd");
        if (cmd == "stand") currentState = STAND;
        else if (cmd == "sit") currentState = SIT;
        else if (cmd == "forward") { currentState = FORWARD; sequenceStep = 0; }
        else if (cmd == "backward") { currentState = BACKWARD; sequenceStep = 0; }
        else if (cmd == "left") { currentState = LEFT; sequenceStep = 0; }
        else if (cmd == "right") { currentState = RIGHT; sequenceStep = 0; }
        else if (cmd == "sweep_on") currentState = SWEEP;
        else if (cmd == "sweep_off" || cmd == "stop") currentState = IDLE;
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Bad Request");
    }
}
