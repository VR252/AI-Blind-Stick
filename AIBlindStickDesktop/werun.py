from ultralytics import YOLO
import cv2
import time
import math
import atexit
from openal import *
from pathlib import Path

# ---------------------------
# OpenAL listener setup
# ---------------------------
listener = oalGetListener()
listener.set_position((0.0, 0.0, 0.0))
listener.set_orientation((0.0, 0.0, -1.0, 0.0, 1.0, 0.0))

# ---------------------------
# Load YOLO model
# ---------------------------
model = YOLO("yolov8n.pt")

# ---------------------------
# Video stream
# ---------------------------
cap = cv2.VideoCapture("http://172.20.10.10/stream")
cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

# ---------------------------
# Load allowed labels
# ---------------------------
allowed_labels = {
    f.stem for f in Path("list_sounds").glob("*.wav")
}

# ---------------------------
# Preload audio sources
# ---------------------------
sources = {}
for label in allowed_labels:
    path = f"list_sounds/{label}.wav"
    src = oalOpen(path)
    src.set_looping(False)
    src.set_gain(1.0)
    sources[label] = src

# ---------------------------
# Settings
# ---------------------------
HORIZONTAL_FOV_DEG = 60.0
distance = 2.0
COOLDOWN = 1.0
last_play_time = 0

# Frame skipping
frame_count = 0
SKIP_FRAMES = 3

# ---------------------------
# Cleanup
# ---------------------------
def shutdown():
    try:
        for src in sources.values():
            src.stop()
            src.destroy()
        oalQuit()
        print("OpenAL cleaned up")
    except:
        pass

atexit.register(shutdown)

# ---------------------------
# Main loop
# ---------------------------
while True:
    ret, frame = cap.read()
    if not ret:
        print("Frame dropped")
        continue

    frame_count += 1
    if frame_count % SKIP_FRAMES != 0:
        continue

    results = model(frame, conf=0.4)

    # Draw detections
    annotated = results[0].plot()
    cv2.imshow("YOLOv8 ESP32", annotated)

    if cv2.waitKey(1) & 0xFF == 27:
        break

    # Cooldown check
    if time.time() - last_play_time < COOLDOWN:
        continue

    # ---------------------------
    # Pick largest valid object
    # ---------------------------
    largest_box = None
    max_area = 0
    chosen_label = None

    for box in results[0].boxes:
        class_id = int(box.cls[0])
        label = results[0].names[class_id]

        if label not in allowed_labels:
            continue

        x1, y1, x2, y2 = map(int, box.xyxy[0])
        area = (x2 - x1) * (y2 - y1)

        if area > max_area:
            max_area = area
            largest_box = (x1, y1, x2, y2)
            chosen_label = label

    # ---------------------------
    # Play sound
    # ---------------------------
    if largest_box is not None:
        x1, y1, x2, y2 = largest_box

        bbox_center_x = (x1 + x2) / 2
        frame_width = frame.shape[1]
        frame_center_x = frame_width / 2

        dx_pixels = bbox_center_x - frame_center_x
        dx_normalized = dx_pixels / frame_center_x

        angle_deg = dx_normalized * (HORIZONTAL_FOV_DEG / 2)

        x = math.sin(math.radians(angle_deg)) * distance
        z = math.cos(math.radians(angle_deg)) * distance
        y = 0.0

        source = sources[chosen_label]
        source.set_position((x, y, z))

        if source.get_state() != AL_PLAYING:
            source.play()
            last_play_time = time.time()

# ---------------------------
# Cleanup on exit
# ---------------------------
cap.release()
cv2.destroyAllWindows()
