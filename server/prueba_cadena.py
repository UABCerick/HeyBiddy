import os, sys, glob
for carpeta in glob.glob(os.path.join(sys.prefix, "Lib", "site-packages", "nvidia", "*", "bin")):
    os.add_dll_directory(carpeta)
    os.environ["PATH"] = carpeta + os.pathsep + os.environ["PATH"]
import time, wave, winsound
import sounddevice as sd
import ollama
from faster_whisper import WhisperModel
from piper import PiperVoice, SynthesisConfig

SR = 16000
sd.default.device = (9, None)
VOCES = {"es": "es_MX-ald-medium.onnx", "en": "en_US-ryan-medium.onnx"}
TONO = 1.30      # 1.0 = normal; más alto = más agudo (prueba 1.2 a 1.45)
LENTITUD = 1.25  # compensa la aceleración; súbelo si habla muy rápido

print("Cargando Whisper...")
stt = WhisperModel("large-v3-turbo", device="cuda", compute_type="float16")

print("Habla 5 segundos...")
audio = sd.rec(5 * SR, samplerate=SR, channels=1, dtype="float32")
sd.wait()

t0 = time.time()
segmentos, info = stt.transcribe(audio[:, 0], vad_filter=True)
texto = " ".join(s.text for s in segmentos).strip()
t1 = time.time()
idioma = info.language if info.language in VOCES else "es"
print(f"[{info.language}] Tú: {texto}")

r = ollama.chat(model="qwen3:14b", think=False, messages=[
    {"role": "system", "content":
        "Eres Biddy, un alienígena pequeño, competitivo y presumido. Hablas con energía, "
        "exageras tus victorias y te burlas en broma, nunca con crueldad. "
        f"Responde en 1-2 frases cortas, en este idioma: {idioma}."},
    {"role": "user", "content": texto},
])
respuesta = r["message"]["content"]
t2 = time.time()
print("Biddy:", respuesta)

voz = PiperVoice.load(VOCES[idioma])
with wave.open("base.wav", "wb") as wf:
    voz.synthesize_wav(respuesta, wf, syn_config=SynthesisConfig(length_scale=LENTITUD))
with wave.open("base.wav", "rb") as src:
    params = src.getparams()
    frames = src.readframes(src.getnframes())
with wave.open("respuesta.wav", "wb") as dst:
    dst.setparams(params)
    dst.setframerate(int(params.framerate * TONO))
    dst.writeframes(frames)
t3 = time.time()

print(f"\nWhisper {t1-t0:.2f}s | LLM {t2-t1:.2f}s | Piper {t3-t2:.2f}s | Total {t3-t0:.2f}s")
winsound.PlaySound("respuesta.wav", winsound.SND_FILENAME)