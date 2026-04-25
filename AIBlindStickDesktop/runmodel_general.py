from ultralytics import YOLO
import cv2
import time
import math
import atexit
import pyttsx3
from openal import *
from pathlib import Path
from queue import Queue
import threading

engine = pyttsx3.init("sapi5")
tts_lock = threading.Lock()


def speak_to_file(text, filename):
    with tts_lock:
        engine.save_to_file(text, filename)
        engine.runAndWait()

#Audio file creation task
audio_queue = Queue()
def audio_worker():
    while True:
        label, file_path = audio_queue.get()
        if label is None:
            break
        speak_to_file(label, file_path)
        audio_queue.task_done()

threading.Thread(target=audio_worker, daemon=True).start()



listener = oalGetListener()
listener.set_position((0.0, 0.0, 0.0))
listener.set_orientation((0.0, 0.0, -1.0, 0.0, 1.0,  0.0))  # up




def shutdown():
    try:
        source.stop()
        source.destroy()
        oalQuit()
        print("OpenAL cleaned up")
    except:
        pass

atexit.register(shutdown)




model = YOLO("yolov8n.pt")
#cap = cv2.VideoCapture("http://192.168.2.250/stream")
cap = cv2.VideoCapture("http://172.20.10.10/stream")


WORD = "person"
source = oalOpen(f"sounds/{WORD}.wav")
source.set_looping(False)
source.set_gain(1.0)
HORIZONTAL_FOV_DEG = 60.0 
distance = 2.0       # meters

COOLDOWN = 1.0 
last_play_time = 0
while True:
    ret, frame = cap.read()
    if not ret:
        break
    

    results = model(frame, conf=0.4)
    annotated = results[0].plot()

    cv2.imshow("YOLOv8 ESP32", annotated)
    if cv2.waitKey(1) & 0xFF == 27:
        break
    if time.time() - last_play_time > COOLDOWN:    
        for box in results[0].boxes:
            x1, y1, x2, y2 = map(int, box.xyxy[0])
            # Bounding box center
            bbox_center_x = (x1 + x2) / 2
            # Frame center
            frame_width = frame.shape[1]
            frame_center_x = frame_width / 2
            # Normalized horizontal offset
            dx_pixels = bbox_center_x - frame_center_x
            dx_normalized = dx_pixels / frame_center_x
            # Angle relative to camera forward direction
            angle_deg = dx_normalized * (HORIZONTAL_FOV_DEG / 2)
            
            x = math.sin(math.radians(angle_deg)) * distance
            z = math.cos(math.radians(angle_deg)) * distance
            y = 0.0
            


            class_id = int(box.cls[0])
            label = results[0].names[class_id]
            file_path = Path(f"sounds/{label}.wav")
            if not file_path.exists():
                audio_queue.put((label, f"sounds/{label}.wav"))

            if file_path.exists():
                source = oalOpen(f"sounds/{label}.wav")
                source.set_looping(False)
                source.set_gain(1.0)
                source.set_position((x, y, z))
                if source.get_state() != AL_PLAYING:
                    source.play()
                last_play_time = time.time()


cap.release()
cv2.destroyAllWindows()


#py -3.11 ./runmodel_general.py
