import socket
import time

UDP_PORT = 8888  # ESP32 is sending here
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", UDP_PORT))

print(f"Listening for RFID tags on UDP port {UDP_PORT}...")

while True:
    data, addr = sock.recvfrom(1024)
    tag = data.decode()
    print(f"Tag received from {addr}: {tag}")

    # Send back a message to ESP32
    reply_msg = "LED_GREEN_ON"
    sock.sendto(reply_msg.encode(), addr)
    print(f"Sent reply to {addr}: {reply_msg}")

