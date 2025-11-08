import numpy as np
import matplotlib.pyplot as plt

# === Parámetros importados de signal_parameters.h ===

SENSOR_SAMPLE_RATE_HZ = 100  # Ajustado a la frecuencia real de adquisición
EPA_ALPHA_PRIMARY = 0.3
EPA_ALPHA_SECONDARY = 0.2
DERIVATIVE_WINDOW_SIZE = 50  # Ventana más grande para derivada (500 ms a 100 Hz)
DERIVATIVE_THRESHOLD_PER_SEC = 120000.0  # Umbral aún más alto para filtrar eventos espurios
DERIVATIVE_THRESHOLD = DERIVATIVE_THRESHOLD_PER_SEC / SENSOR_SAMPLE_RATE_HZ
DERIVATIVE_FILTER_ALPHA = 0.1

# Carga datos
# Formato: timestamp, valor_raw
csv_file = 'lecturas.csv'
data = np.loadtxt(csv_file, delimiter=',', skiprows=1)
timestamps = data[:, 0]
raw = data[:, 1]

# Filtro EPA primario
primary_filtered = np.zeros_like(raw)
primary_filtered[0] = raw[0]
for i in range(1, len(raw)):
    primary_filtered[i] = EPA_ALPHA_PRIMARY * raw[i] + (1 - EPA_ALPHA_PRIMARY) * primary_filtered[i-1]

# Filtro EPA secundario
secondary_filtered = np.zeros_like(raw)
secondary_filtered[0] = primary_filtered[0]
for i in range(1, len(raw)):
    secondary_filtered[i] = EPA_ALPHA_SECONDARY * primary_filtered[i] + (1 - EPA_ALPHA_SECONDARY) * secondary_filtered[i-1]


# Derivada con ventana sobre el primer EPA
window_derivative_primary = np.zeros_like(primary_filtered)
for i in range(DERIVATIVE_WINDOW_SIZE, len(primary_filtered)):
    window_derivative_primary[i] = (primary_filtered[i] - primary_filtered[i - DERIVATIVE_WINDOW_SIZE]) / DERIVATIVE_WINDOW_SIZE


# Derivada con ventana sobre el segundo EPA (doble EPA)
window_derivative = np.zeros_like(secondary_filtered)
for i in range(DERIVATIVE_WINDOW_SIZE, len(secondary_filtered)):
    window_derivative[i] = (secondary_filtered[i] - secondary_filtered[i - DERIVATIVE_WINDOW_SIZE]) / DERIVATIVE_WINDOW_SIZE



# Derivada filtrada (EPA sobre la derivada con ventana del primer EPA)
filtered_derivative_primary = np.zeros_like(window_derivative_primary)
filtered_derivative_primary[0] = window_derivative_primary[0]
for i in range(1, len(window_derivative_primary)):
    filtered_derivative_primary[i] = DERIVATIVE_FILTER_ALPHA * window_derivative_primary[i] + (1 - DERIVATIVE_FILTER_ALPHA) * filtered_derivative_primary[i-1]


# Derivada filtrada (EPA sobre la derivada con ventana)
filtered_derivative = np.zeros_like(window_derivative)
filtered_derivative[0] = window_derivative[0]
for i in range(1, len(window_derivative)):
    filtered_derivative[i] = DERIVATIVE_FILTER_ALPHA * window_derivative[i] + (1 - DERIVATIVE_FILTER_ALPHA) * filtered_derivative[i-1]





# --- Parámetros de contexto de evento (importados de signal_parameters.h) ---

# --- Parámetros de contexto de evento (importados de signal_parameters.h) ---
PRE_EVENT_PERIOD_MS = 400  # Compensar retardo de filtros (400 ms delante)
POST_EVENT_PERIOD_MS = 0   # No añadir post-evento
SENSOR_SAMPLE_INTERVAL_MS = 10  # 100 Hz = 10 ms por muestra
import math
PRE_EVENT_SAMPLES = math.ceil(PRE_EVENT_PERIOD_MS / SENSOR_SAMPLE_INTERVAL_MS)
POST_EVENT_SAMPLES = math.ceil(POST_EVENT_PERIOD_MS / SENSOR_SAMPLE_INTERVAL_MS)

# Detección de eventos (1 si derivada filtrada > threshold, 0 si no)
event_flag = (np.abs(filtered_derivative) > DERIVATIVE_THRESHOLD).astype(int)

# --- Construcción de máscara de tramos: 0=estable, 1=evento, 2=pre, 3=post ---
segment_mask = np.zeros_like(event_flag)

# Detectar inicios y finales de eventos
in_event = False
event_segments = []  # (start_idx, end_idx)
for i, flag in enumerate(event_flag):
    if not in_event and flag:
        event_start = i
        in_event = True
    elif in_event and not flag:
        event_end = i-1
        event_segments.append((event_start, event_end))
        in_event = False
# Si termina en evento
if in_event:
    event_segments.append((event_start, len(event_flag)-1))



# --- Cálculo de indicadores clave por evento ---
print(f"PRE_EVENT_SAMPLES: {PRE_EVENT_SAMPLES}, POST_EVENT_SAMPLES: {POST_EVENT_SAMPLES}")
event_indicators = []
for idx, (start, end) in enumerate(event_segments):
    pre_start = max(0, start - PRE_EVENT_SAMPLES)
    print(f"Evento {idx}: pre [{pre_start}:{start-1}], evento [{start}:{end}]")
    segment_mask[pre_start:start] = 2
    segment_mask[start:end+1] = 1

    # --- Extraer datos del tramo (evento + pre) ---
    seg_idx = np.arange(pre_start, end+1)
    seg_time = timestamps[seg_idx]
    seg_raw = raw[seg_idx]
    seg_filtered = secondary_filtered[seg_idx]
    seg_deriv = np.gradient(seg_raw, seg_time)

    # 1) Cambio de presión total (ΔP)
    delta_p = seg_raw[-1] - seg_raw[0]
    # 2) Pico máximo de presión (overshoot)
    overshoot = np.max(seg_raw)
    # 3) Tiempo hasta el pico (t_peak)
    t_peak = seg_time[np.argmax(seg_raw)] - seg_time[0]
    # 4) Pendiente máxima de subida
    max_slope = np.max(seg_deriv)
    # 5) Pendiente máxima de bajada
    min_slope = np.min(seg_deriv)
    # 6) Tiempo de estabilización (t_stabilize):
    # Definimos estabilización como cuando la señal se mantiene dentro del 5% del valor final
    final_val = seg_raw[-1]
    tol = 0.05 * np.abs(final_val)
    stable_idx = np.where(np.abs(seg_raw - final_val) < tol)[0]
    t_stabilize = (seg_time[stable_idx[0]] - seg_time[0]) if len(stable_idx) > 0 else np.nan
    # 7) Frecuencia dominante (FFT)
    from scipy.fft import rfft, rfftfreq
    N = len(seg_raw)
    if N > 1:
        yf = np.abs(rfft(seg_raw - np.mean(seg_raw)))
        xf = rfftfreq(N, d=SENSOR_SAMPLE_INTERVAL_MS/1000)
        dom_freq = xf[np.argmax(yf[1:])+1] if len(yf) > 1 else 0
        # 8) Energía de alta frecuencia (por encima de 10 Hz)
        hf_energy = np.sum(yf[xf > 10]**2)
    else:
        dom_freq = 0
        hf_energy = 0
    # 9) Área bajo la curva (impulso total)
    area = np.trapz(seg_raw, seg_time)

    indicators = {
        'ΔP': delta_p,
        'overshoot': overshoot,
        't_peak': t_peak,
        'max_slope': max_slope,
        'min_slope': min_slope,
        't_stabilize': t_stabilize,
        'dom_freq': dom_freq,
        'hf_energy': hf_energy,
        'area': area
    }
    event_indicators.append(indicators)



# Gráfica con subplots
fig, axs = plt.subplots(3, 1, figsize=(14, 10), sharex=True)


# Primer gráfico: señal original y doble EPA, coloreando tramos y añadiendo marcas verticales
colors = {0: 'gray', 1: 'red', 2: 'blue', 3: 'green'}
labels = {0: 'Estable', 1: 'Evento', 2: 'Pre-evento', 3: 'Post-evento'}
for seg_type in [2, 0, 3, 1]:  # Orden: pre, estable, post, evento
    mask = segment_mask == seg_type
    if np.any(mask):
        axs[0].plot(timestamps[mask], secondary_filtered[mask], '.', color=colors[seg_type], label=labels[seg_type], alpha=0.7)
axs[0].plot(timestamps, raw, label="Señal original (RAW)", color="black", alpha=0.3)
# Mostrar escala de tiempo en segundos en el eje x
from matplotlib.ticker import FuncFormatter
def ms_to_s(x, pos):
    return f"{x/1000:.1f}"
axs[0].set_title("Señal original y doble EPA (coloreado por tramo)")
axs[0].xaxis.set_major_formatter(FuncFormatter(ms_to_s))
axs[0].set_xlabel('Tiempo (s)')


# Añadir marcas verticales y texto de indicadores para cada evento
for idx, (start, end) in enumerate(event_segments):
    pre_start = max(0, start - PRE_EVENT_SAMPLES)
    axs[0].axvline(timestamps[pre_start], color='blue', linestyle='--', alpha=0.5)
    axs[0].axvline(timestamps[start], color='red', linestyle='-', alpha=0.7)
    axs[0].axvline(timestamps[end], color='red', linestyle='-', alpha=0.7)
    # Texto con indicadores clave
    ind = event_indicators[idx]
    txt = (f"ΔP={ind['ΔP']:.0f}\n"
        f"peak={ind['overshoot']:.0f}\n"
        f"t_peak={ind['t_peak']:.0f}ms\n"
        f"max_slope={ind['max_slope']:.0f}\n"
        f"min_slope={ind['min_slope']:.0f}\n"
        f"t_stab={ind['t_stabilize']:.0f}ms\n"
        f"f_dom={ind['dom_freq']:.1f}Hz\n"
        f"hfE={ind['hf_energy']:.0e}\n"
        f"area={ind['area']:.1e}")
    # Colocar el texto cerca del centro del evento
    x_txt = timestamps[start] + (timestamps[end]-timestamps[start])/2
    y_txt = np.max(raw) * 0.98
    axs[0].text(x_txt, y_txt, txt, fontsize=8, va='top', ha='center', bbox=dict(facecolor='white', alpha=0.7, edgecolor='none'))

axs[0].legend()

# Segundo gráfico: derivada ventana
axs[1].plot(timestamps, window_derivative, color='tab:orange', label='Derivada ventana (doble EPA, win=10)')
axs[1].set_title('Derivada con ventana (doble EPA)')
axs[1].legend()

# Tercer gráfico: derivada filtrada y eventos
axs[2].plot(timestamps, filtered_derivative, color='tab:orange', label='Derivada filtrada (doble EPA)')
axs[2].plot(timestamps, event_flag * np.max(filtered_derivative), color='tab:red', alpha=0.5, label='Evento detectado')
axs[2].set_title('Derivada filtrada y eventos')
axs[2].legend()
axs[2].set_xlabel('Timestamp (ms)')

plt.tight_layout()
plt.show()
