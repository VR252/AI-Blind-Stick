import serial
import time


port = "COM7" 
baudrate = 115200 

ser = serial.Serial(port, baudrate, timeout=5)
print(f"Connected to {port}. Waiting for image...")

while True:
    # Look for the START marker
    ser.reset_input_buffer()
    if ser.read(5) == b'START':
        print("Receiving image...")
        
        # Read the 4-byte size header
        size_bytes = ser.read(4)
        image_size = int.from_bytes(size_bytes, byteorder='little')
        
        # Read the actual image data
        image_data = ser.read(image_size)
        
        # Save to a file
        with open("received_capture.jpg", "wb") as f:
            f.write(image_data)
        
        print(f"Saved received_capture.jpg ({image_size} bytes)")