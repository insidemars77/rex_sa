import serial
import threading

rex = serial.Serial("COM5", 115200, timeout=1)

def receive():
    while True:
        try:
            data = rex.readline().decode(errors="ignore").strip()
            if data:
                print(f"\nRex > {data}")
                print("PC > ", end="", flush=True)
        except:
            break

threading.Thread(target=receive, daemon=True).start()

print("Rex connected.")
print("Type messages and press Enter.\n")

while True:
    message = input("PC > ")

    if message.lower() == "exit":
        break

    rex.write((message + "\n").encode())

rex.close()