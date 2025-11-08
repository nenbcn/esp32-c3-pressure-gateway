import serial
import csv

SERIAL_PORT = '/dev/tty.usbmodem1101'  # Cambia si tu dispositivo tiene otro sufijo
BAUDRATE = 115200
CSV_FILE = 'lecturas.csv'

with serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1) as ser, open(CSV_FILE, 'w', newline='') as csvfile:
    writer = csv.writer(csvfile)
    writer.writerow(['timestamp_ms', 'valor_raw'])  # Cabecera opcional
    print("Grabando datos... Pulsa Ctrl+C para parar.")
    try:
        while True:
            line = ser.readline().decode('utf-8').strip()
            if line and ',' in line:
                parts = line.split(',')
                if len(parts) == 2:
                    try:
                        timestamp = int(parts[0].strip())
                        valor = int(parts[1].strip())
                        writer.writerow([timestamp, valor])
                        print(timestamp, valor)
                    except ValueError:
                        pass  # Línea corrupta, ignora
    except KeyboardInterrupt:
        print("\nGrabación finalizada.")
