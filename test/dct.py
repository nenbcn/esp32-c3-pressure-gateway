import serial
import numpy as np
import matplotlib.pyplot as plt
from scipy.fftpack import dct

SERIAL_PORT = '/dev/cu.usbmodem1101'
BAUD_RATE = 115200
NUM_SAMPLES = 128
SAMPLING_FREQ = 100  # Hz (ajusta si tu micro envía a otra frecuencia)

def get_samples():
    samples = []
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2) as ser:
        print(f"Collecting {NUM_SAMPLES} samples...")
        while len(samples) < NUM_SAMPLES:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if line:
                print(f"Linea recibida: {line}")  # Depuración
            if ',' in line and line[0].isdigit():
                _, raw_str = line.split(',', 1)
                raw = int(raw_str)
                samples.append(raw)
                if len(samples) % 100 == 0:
                    print(f"{len(samples)} muestras recogidas")
    print("Done.")
    return np.array(samples)

def main():
    samples = get_samples()
    # DCT tipo II
    dct_coeffs = dct(samples, norm='ortho')
    # Frecuencias asociadas (Hz)
    freqs = np.arange(NUM_SAMPLES) * SAMPLING_FREQ / (2 * NUM_SAMPLES)
    # Mostrar coeficiente DC (k=0) aparte
    print(f"Coeficiente DC (k=0, 0 Hz): {dct_coeffs[0]:.2f}")
    # Gráfico de barras de los coeficientes (sin el DC)
    plt.figure(figsize=(14, 6))
    plt.bar(freqs[1:], np.abs(dct_coeffs[1:]), width=freqs[1]-freqs[0], color='royalblue')
    plt.title('DCT Coefficients of Pressure Sensor RAW Data (excluding DC)')
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('DCT Coefficient Magnitude')
    plt.xlim(0, 10)  # Muestra hasta 10 Hz, ajusta si lo necesitas
    plt.grid(True)
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()
