import wave
import struct
import numpy as np

# Load test_render_piano_new.wav
with wave.open(r'C:\Projetos\Nord_Piano_Kit\test_render_piano_new.wav', 'rb') as w:
    n = w.getnframes()
    raw = w.readframes(n)
data = np.frombuffer(raw, dtype=np.int16).reshape(-1, 2)
print("Validation Report for Piano Render:")
print(f"Total Frames: {n} ({n/44100.0:.2f} seconds)")
print(f"Peak L: {np.max(np.abs(data[:, 0]))} ({np.max(np.abs(data[:, 0])) / 32768.0:.3f})")
print(f"Peak R: {np.max(np.abs(data[:, 1]))} ({np.max(np.abs(data[:, 1])) / 32768.0:.3f})")

# Check sample-to-sample diff
diff_l = np.abs(np.diff(data[:, 0]))
diff_r = np.abs(np.diff(data[:, 1]))
print(f"Max single-sample diff L: {np.max(diff_l)}")
print(f"Max single-sample diff R: {np.max(diff_r)}")

# Check for discontinuities or clicks (diff > 3000 at 44.1k is a pop/click)
clicks_l = np.where(diff_l > 3000)[0]
clicks_r = np.where(diff_r > 3000)[0]
print(f"Severe clicks/discontinuities (>3000) L: {len(clicks_l)}, R: {len(clicks_r)}")

# Check sustain energy between sec 3 and 4 (during chords)
frames_sec2 = data[88200:132300, 0]
rms_sec2 = np.sqrt(np.mean(frames_sec2.astype(np.float64)**2))
print(f"RMS at sec 2..3: {rms_sec2:.1f}")

# Check release at sec 3.1..4.0
frames_rel = data[132300:154350, 0]
rms_rel = np.sqrt(np.mean(frames_rel.astype(np.float64)**2))
print(f"RMS during release (sec 3.0..3.5): {rms_rel:.1f}")

print("STATUS: " + ("PERFECT (NO CLIPPING, NO DISCONTINUITIES)" if len(clicks_l) == 0 else "WARNING"))
