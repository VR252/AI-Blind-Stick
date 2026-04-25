Markdown
# 🦯 AI Blind-Stick
> An intelligent assistive mobility device combining real-time object detection and proximity sensing to empower the visually impaired.

[![Hardware](https://img.shields.io/badge/Hardware-ESP32--WROVER-orange.svg)](#-hardware-stack)
[![AI-Model](https://img.shields.io/badge/AI-YOLOv8-blue.svg)](#-software--ai-stack)
[![Field](https://img.shields.io/badge/Field-Assistive%20Tech-green.svg)](#)

## 🌟 Project Overview
Traditional white canes fail to detect hanging obstacles or hazards beyond immediate physical contact. The **AI Blind-Stick** bridges this gap by acting as a "second set of eyes." It utilizes an integrated camera and ultrasonic sensors to provide users with a comprehensive mental map of their surroundings through haptic feedback and 3D spatial audio.

### Key Features
* **🔊 3D Spatial Audio:** Converts visual data into directional sound, telling the user *what* an object is and *where* it is located.
* **📳 Intelligent Haptics:** Variable vibration intensity on the handle based on real-time distance measurements (20cm to 150cm).
* **👁️ AI Object Recognition:** Real-time identification of hazards like stairs, people, and vehicles using the YOLOv8 architecture.
* **📶 Remote Processing:** Hybrid architecture that streams video from the ESP32 to a workstation for high-speed inference.

---
<img width="658" height="465" alt="Screenshot 2026-04-25 163609" src="https://github.com/user-attachments/assets/5e4998bd-4ec0-47e0-b294-5374380c7ce0" />



## 🏗️ System Architecture

### 🔧 Hardware Stack
| Component | Purpose |
| :--- | :--- |
| **ESP32-WROVER** | Central microcontroller with Wi-Fi/Bluetooth capabilities. |
| **OV3660 Camera** | Captures environmental visual data. |
| **HC-SR04** | Ultrasonic sensor for low-level proximity detection. |
| **L293D + DC Motor** | Provides haptic feedback via the stick handle. |
| **Buck Converter** | Regulates power from 4x AA batteries to 5V system voltage. |

### 💻 Software & AI Stack
- **Firmware:** ESP-IDF (C/C++)
- **Computer Vision:** OpenCV & Python
- **AI Model:** YOLOv8n (Nano) - Optimized for low latency.
- **Audio Engine:** OpenAL (for 3D sound spatialization).

---

## 🚀 How It Works

### 1. The Haptic Loop (Proximity)
The ultrasonic sensor measures the time-of-flight for sound waves. The system calculates distance and maps it to a vibration frequency:
- **Long Range:** Slow pulses.
- **Critical Range (<30cm):** Continuous high-frequency vibration.

### 2. The Vision Loop (Object Detection)
1. **Capture:** ESP32 captures frames and streams them over a local network.
2. **Analysis:** The Python backend runs the YOLOv8 model on each frame.
3. **Audio Mapping:**
    - If an object is detected on the **left**, the audio cue pans to the **left ear**.
    - The **pitch** or **volume** changes based on object proximity.

---

## 📊 Performance Metrics
- **Detection Accuracy:** ≥ 90% in tested environments.
- **Sensor Precision:** High-reliability distance sensing with $\geq 99\%$ accuracy.
- **Latency:** Optimized for real-time response using the YOLOv8n lightweight model.
