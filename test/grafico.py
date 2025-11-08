import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

SERIAL_PORT = '/dev/cu.usbmodem1101'
BAUD_RATE = 115200
MAX_POINTS = 2000
WINDOW_SECONDS = 20

Y_MIN = 1500000
Y_MAX = 4000000

timestamps = deque(maxlen=MAX_POINTS)
raws = deque(maxlen=MAX_POINTS)
double_ema_filtered = deque(maxlen=MAX_POINTS)
raw_timestamps = deque(maxlen=MAX_POINTS)  # Timestamps absolutos en ms
start_ts = None

# Parámetros para filtro exponencial doble (asimétrico)
fast_alpha = 0.25   # Más rápido, sigue mejor la RAW
slow_alpha = 0.08    # Más rápido, pero aún suaviza el ruido
edge_threshold = 12000  # Umbral robusto para cambios reales (ajusta según tu gráfica)

# Lists to store edge start and end times (en segundos absolutos)
edge_starts_abs = []
edge_ends_abs = []
edge_active = False

# Añade esta lista global al inicio del archivo
edge_events_log = []
vertical_lines = []  # Añade esto al inicio del archivo
MIN_STABLE_TIME = 1.5   # segundos mínimos en estado estable antes de aceptar nuevo cambio
MIN_EDGE_TIME = 1.5     # segundos mínimos en estado de cambio antes de aceptar vuelta a estable
FRAMES_REQUIRED = 8     # número de frames consecutivos para confirmar cambio
last_edge_end_time = 0
last_edge_start_time = 0

# Añade al inicio:
stable_counter = 0
change_counter = 0

def read_serial_data(ser):
    global start_ts
    latest_raw = None

    while ser.in_waiting:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if ',' in line and line[0].isdigit():
            ts_str, raw_str = line.split(',', 1)
            ts = int(ts_str)
            raw = int(raw_str)
            latest_raw = raw
            if start_ts is None:
                start_ts = ts
            # Tiempo relativo en segundos desde el primer dato recibido
            t = (ts - start_ts) / 1000.0
            timestamps.append(t)
            raw_timestamps.append(ts)  # <-- Guarda el timestamp absoluto
            raws.append(raw)

            # Double EMA (asimétrico)
            if len(double_ema_filtered) == 0:
                # Inicializar el filtro con el primer valor
                double_ema_filtered.append(raw)
            else:
                # Obtener valor anterior
                prev_double = double_ema_filtered[-1]
                
                # Calcular filtros rápido y lento
                fast_ema = fast_alpha * raw + (1 - fast_alpha) * prev_double
                slow_ema = slow_alpha * raw + (1 - slow_alpha) * prev_double
                
                # Detectar cambios significativos comparando filtros
                edge_detected = abs(fast_ema - slow_ema) > edge_threshold
                
                if edge_detected:
                    # En transiciones: usar filtro rápido para reducir retardo
                    adaptive_alpha = fast_alpha
                else:
                    # En estado estable: usar filtro lento para más estabilidad
                    adaptive_alpha = slow_alpha
                
                # Aplicar filtro adaptativo
                double_ema_value = adaptive_alpha * raw + (1 - adaptive_alpha) * prev_double
                double_ema_filtered.append(double_ema_value)
    return latest_raw

def setup_plot():
    fig, ax = plt.subplots(figsize=(13, 7))
    line_raw, = ax.plot([], [], 'b-', label='RAW', linewidth=1)
    line_double_ema, = ax.plot([], [], 'green', label=f'Double EMA (α_fast={fast_alpha}, α_slow={slow_alpha})', linewidth=2)
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('RAW Value')
    ax.legend(loc='upper right')
    ax.grid(True, alpha=0.3)
    ax.set_ylim(Y_MIN, Y_MAX)
    value_text = ax.text(0.02, 0.98, '', transform=ax.transAxes, fontsize=12,
                         verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
    # Añade eje secundario para la derivada
    ax2 = ax.twinx()
    line_deriv, = ax2.plot([], [], 'magenta', label='d(Double EMA)/dt', linewidth=1)
    ax2.set_ylabel('Derivative')
    ax2.legend(loc='lower right')
    return fig, ax, ax2, line_raw, line_double_ema, line_deriv, value_text

def update_plot(frame, ser, ax, ax2, line_raw, line_double_ema, line_deriv, value_text):
    global edge_active, last_edge_end_time, last_edge_start_time, stable_counter, change_counter
    raw_value = read_serial_data(ser)
    if not raw_timestamps or len(raw_timestamps) < 2:
        ax.set_title("Waiting for WNK1MA sensor data...")
        value_text.set_text('No data')
        return line_raw, line_double_ema, line_deriv, value_text

    tmax_abs = raw_timestamps[-1] / 1000.0
    tmin_abs = max(0, tmax_abs - WINDOW_SECONDS)

    x = [(ts / 1000.0) for ts in raw_timestamps]
    y_raw = list(raws)
    y_double_ema = list(double_ema_filtered)
    idxs = [i for i, t in enumerate(x) if tmin_abs <= t <= tmax_abs]

    if not idxs:
        ax.set_title("No data in window")
        value_text.set_text('No data in window')
        return line_raw, line_double_ema, line_deriv, value_text

    x_window = [x[i] for i in idxs]
    y_raw_window = [y_raw[i] for i in idxs]
    y_double_ema_window = [y_double_ema[i] for i in idxs]

    # Derivada de la doble EMA
    if len(x_window) >= 2:
        # Derivada suavizada: diferencia entre la media de los 10 anteriores y los 10 siguientes
        N = 10
        deriv_window = []
        x_deriv = []
        if len(x_window) >= 2 * N + 1:
            for i in range(N, len(x_window) - N):
                prev_mean = sum(y_double_ema_window[i-N:i]) / N
                next_mean = sum(y_double_ema_window[i+1:i+1+N]) / N
                dt = x_window[i+N] - x_window[i-N]
                deriv = (next_mean - prev_mean) / dt if dt != 0 else 0
                deriv_window.append(deriv)
                x_deriv.append(x_window[i])
        else:
            deriv_window = []
            x_deriv = []
    else:
        deriv_window = []
        x_deriv = []

    line_deriv.set_data(x_deriv, deriv_window)
    line_raw.set_data(x_window, y_raw_window)
    line_double_ema.set_data(x_window, y_double_ema_window)
    ax.set_xlim(tmin_abs, tmax_abs)
    ax2.set_xlim(tmin_abs, tmax_abs)

    # Línea horizontal en el gráfico principal a 335500
    ax.axhline(y=335500, color='purple', linestyle=':', linewidth=2, label='Threshold 335500')

    # Escala del eje derivada x5 respecto a la original
    ax2.set_ylim(-1000000, 1000000)  # Escala x5 respecto a -100000, 100000

    # --- Detección de cambios significativos ---
    if len(double_ema_filtered) >= 2:
        last_raw = raws[-1]
        prev_double = double_ema_filtered[-2]
        fast_ema = fast_alpha * last_raw + (1 - fast_alpha) * prev_double
        slow_ema = slow_alpha * last_raw + (1 - slow_alpha) * prev_double
        edge_diff = abs(fast_ema - slow_ema)
        edge_detected = edge_diff > edge_threshold

        current_time_abs = raw_timestamps[-1] / 1000.0

        # Histéresis por frames consecutivos
        if edge_detected:
            change_counter += 1
            stable_counter = 0
        else:
            stable_counter += 1
            change_counter = 0

        # Solo marca EDGE START si llevamos suficiente tiempo en estado estable
        if edge_detected and not edge_active and change_counter >= FRAMES_REQUIRED and (current_time_abs - last_edge_end_time > MIN_STABLE_TIME):
            edge_starts_abs.append(current_time_abs)
            edge_active = True
            last_edge_start_time = current_time_abs
            edge_events_log.append(f"EDGE START at {current_time_abs:.2f}s")
        # Solo marca EDGE END si llevamos suficiente tiempo en estado de cambio
        elif not edge_detected and edge_active and stable_counter >= FRAMES_REQUIRED and (current_time_abs - last_edge_start_time > MIN_EDGE_TIME):
            edge_ends_abs.append(current_time_abs)
            edge_active = False
            last_edge_end_time = current_time_abs
            edge_events_log.append(f"EDGE END at {current_time_abs:.2f}s")

    # Muestra los últimos 10 eventos en el cuadro de texto
    events_text = "\n".join(edge_events_log[-10:])

    value_text.set_text(
        f"RAW: {raw_value if raw_value else '-'}\n"
        f"DoubleEMA: {int(y_double_ema_window[-1]) if y_double_ema_window else '-'}\n"
        f"Time: {tmax_abs:.1f}s\n"
        f"{events_text}"
    )
    ax.set_title(f"WNK1MA Pressure Sensor - Real Time Monitoring ({len(x_window)} samples)")

    # Elimina líneas verticales previas
    for vline in vertical_lines:
        vline.remove()
    vertical_lines.clear()

    # Dibuja líneas verticales para EDGE START (rojo) y EDGE END (naranja) dentro de la ventana visible
    for t in edge_starts_abs:
        if tmin_abs <= t <= tmax_abs:
            vline = ax.axvline(x=t, color='red', linestyle='--', linewidth=1.5, label='EDGE START')
            vertical_lines.append(vline)
    for t in edge_ends_abs:
        if tmin_abs <= t <= tmax_abs:
            vline = ax.axvline(x=t, color='orange', linestyle='--', linewidth=1.5, label='EDGE END')
            vertical_lines.append(vline)

    # Actualiza la leyenda sin duplicados
    handles, labels = ax.get_legend_handles_labels()
    unique = dict(zip(labels, handles))
    ax.legend(unique.values(), unique.keys())

    return line_raw, line_double_ema, line_deriv, value_text

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to {SERIAL_PORT} for WNK1MA sensor monitoring")
        print(f"Double EMA Filter - Fast α: {fast_alpha}, Slow α: {slow_alpha}, Edge threshold: {edge_threshold}")
    except Exception as e:
        print(f"Serial connection error: {e}")
        return

    fig, ax, ax2, line_raw, line_double_ema, line_deriv, value_text = setup_plot()
    ani = animation.FuncAnimation(
        fig, update_plot,
        fargs=(ser, ax, ax2, line_raw, line_double_ema, line_deriv, value_text),
        interval=50,
        blit=False,
        cache_frame_data=False
    )
    plt.tight_layout()
    plt.show()
    ser.close()

if __name__ == "__main__":
    main()