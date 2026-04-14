# 🕷️ Hex-Spider v2.0: Advanced Hexapod Controller

An ESP32-powered spider robot featuring a custom **Glassmorphism Web Interface**, a **Tripod Walking Gait**, and specialized movement sequences like **Dance** and **Attack** modes.

---

## 📸 Gallery

| 🤖 Robot Model | 🧪 Testing Phase | 🌐 Web Interface |
| :---: | :---: | :---: |
| ![Robot Model](images/robotmodel.jpg) | ![Testing Phase](images/testingphase.jpg) | ![Web Interface](images/website.jpg) |

---

## 🚀 Features

* **Wireless Control:** Host a local WiFi Access Point to control the robot from any smartphone or PC browser.
* **Tripod Walking Gait:** Efficient 6-legged movement logic for stable forward, backward, and lateral traversal.
* **Modern UI:** A clean, mobile-responsive "Glassmorphism" control hub with a directional D-PAD.
* **Special Sequences:** * **Dance Mode:** A rhythmic multi-step sequence with high-speed servo interpolation.
    * **Attack Pose:** Aggressive front-leg striking posture.
    * **Wiggle:** A playful high-frequency hip oscillation.
* **Smooth Motion:** Non-blocking servo interpolation using the `PCA9685` driver for fluid, lifelike movement.

---

## 🛠️ Hardware Requirements

* **Microcontroller:** ESP32 (WROOM-32)
* **Servo Driver:** Adafruit PCA9685 16-Channel PWM Shield
* **Servos:** 12x MG90S or SG90 Micro Servos
* **Power:** 7.4V LiPo Battery (with Buck Converter for 5V/6V Servo power)
* **Communication:** I2C (Pins 21 SDA, 22 SCL)

---

## 💻 Software Setup

1.  **Libraries:** Ensure you have the following libraries installed in your Arduino IDE:
    * `WiFi.h`
    * `WebServer.h`
    * `Wire.h`
    * `Adafruit_PWMServoDriver.h`
2.  **Configuration:** * Default SSID: `SpiderRobot_Pro`
    * Default Password: `12345678`
    * Default IP: `192.168.4.1`
3.  **Upload:** Flash the provided `.ino` code to your ESP32.
4.  **Connect:** Connect your phone to the "SpiderRobot_Pro" WiFi and navigate to `http://192.168.4.1` in your browser.

---

## 📐 Motion Logic

The robot utilizes a **State Machine** architecture to handle overlapping movements:
- **`updateServoPositions()`**: Manages the "Ease-In/Ease-Out" effect by stepping angles toward a target rather than jumping instantly.
- **`executeSequences()`**: Manages the timing and gait phases for walking and dancing.

---

## Contributor:

- ItzVoidTech (Swarnendu Kundu)
