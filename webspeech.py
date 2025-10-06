import os
from flask import Flask, request
import speech_recognition as sr

app = Flask(__name__)

# Folder to save uploaded files
UPLOAD_FOLDER = "uploads"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# Path for the single recording file
UPLOAD_PATH = os.path.join(UPLOAD_FOLDER, "recording.wav")

# Keywords to detect
KEYWORDS = ["weather", "time", "calendar", "trash"]

@app.route("/", methods=["GET"])
def index():
    return "ESP32 Audio Upload Server Running!"

@app.route("/upload", methods=["POST"])
def upload_audio():
    # ESP32 sends raw bytes, so we read request.data
    audio_data = request.data
    if not audio_data:
        return "No data received", 400

    # Save WAV file
    with open(UPLOAD_PATH, "wb") as f:
        f.write(audio_data)

    print(f"[INFO] File received: {UPLOAD_PATH}")

    # Transcribe using SpeechRecognition
    recognizer = sr.Recognizer()
    try:
        with sr.AudioFile(UPLOAD_PATH) as source:
            audio = recognizer.record(source)
            text = recognizer.recognize_google(audio)
            print(f"[TRANSCRIPTION]: {text}")

            # Check for keywords (case-insensitive)
            text_lower = text.lower()
            for kw in KEYWORDS:
                if kw in text_lower:
                    print(f"[KEYWORD DETECTED]: {kw}")
                    return kw, 200  # return the detected keyword

            # If no keyword detected, return 'none'
            return "none", 200

    except sr.UnknownValueError:
        return "Could not understand audio", 400
    except sr.RequestError as e:
        return f"SpeechRecognition API error: {e}", 500

if __name__ == "__main__":
    # Host accessible over local network
    app.run(host="0.0.0.0", port=5000)
