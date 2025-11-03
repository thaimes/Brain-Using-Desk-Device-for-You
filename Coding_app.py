import os, ctypes, io, sys, calendar, threading
from datetime import date
from queue import Queue, Empty

from google import genai
from flask import Flask, request
import speech_recognition as sr

# ---- Tk / GUI ----
import tkinter as tk
from tkinter import messagebox
from PIL import Image, ImageTk, ImageDraw

# High-DPI fix (Windows)
try:
    ctypes.windll.shcore.SetProcessDpiAwareness(1)   # Per-monitor DPI aware
except Exception:
    pass

# Config 
BASE_W, BASE_H = 2560, 1440  # Your design reference size
ASSETS = r"C:\Users\tiny5\OneDrive\Desktop\Code_app\assets" # Replace with asset PATH file location
BG_PATH = os.path.join(ASSETS, "background.png")
CARD_PATH = os.path.join(ASSETS, "title.png")   # your wide pill card art
MASCOT_PATH = os.path.join(ASSETS, "mascot.png")

# Gemini configuration
API_KEY = "APIKEY" # Message Thomas for key
client = genai.Client(api_key = API_KEY)
questionflag = False
last_transcript = None
last_response = None

learning = False

#10-lesson path (toward tank control)
# Hide solutions (use scaffolds instead of solved starters)
HIDE_ANSWERS = True
LESSONS = {
    "01 • Say Hello": {
        "desc": "Print your first message. Change the text to make Python talk.",
        "starter": "print('Hello, world!')",
        "check": lambda out: 'Hello' in out,
        "success": "Nice! Your program talked 🎉",
    },
    "02 • Variables": {
        "desc": "Store your name in a variable and use it in a sentence.",
        "starter": "name = 'Coder'\nprint('Hello, ' + name + '!')",
        "check": lambda out: 'Hello' in out and '!' in out,
        "success": "Great—your variable printed correctly!",
    },
    "03 • Math & Print": {
        "desc": "Do a little math with variables and print the answer.",
        "starter": "a = 7\nb = 5\nprint('Sum:', a + b)  # Try changing a and b",
        "check": lambda out: "Sum:" in out,
        "success": "Math success! You printed a calculation.",
    },
    "04 • If (Decisions)": {
        "desc": "Print PASS if score is 70 or more, otherwise print TRY AGAIN.",
        "starter": "score = 75  # try changing this number\nif score >= 70:\n    print('PASS')\nelse:\n    print('TRY AGAIN')",
        "check": lambda out: ("PASS" in out) or ("TRY AGAIN" in out),
        "success": "Decision made! Your if/else works!",
    },
    "05 • For Loop": {
        "desc": "Use a loop to print numbers 1 to 5 on separate lines.",
        "starter": "for i in range(1,6):\n    print(i)",
        "check": lambda out: all(str(n) in out for n in range(1,6)),
        "success": "Loop-tastic! You counted with a for-loop.",
    },
    "06 • While Loop": {
        "desc": "Make a countdown from 3 to 1, then print GO!",
        "starter": "n = 3\nwhile n > 0:\n    print(n)\n    n -= 1\nprint('GO!')",
        "check": lambda out: "GO!" in out,
        "success": "Blast off! Your while-loop counted down.",
    },
    "07 • Functions": {
        "desc": "Make a function and call it. This prepares us for movement logic.",
        "starter": "def shout(msg):\n    return msg.upper() + '!'\n\nprint(shout('ready'))",
        "check": lambda out: "READY!" in out,
        "success": "Function power! You created and called a function.",
    },
    "08 • Lists (Moves)": {
        "desc": "Store steps in a list and loop through them (left/right/up/down).",
        "starter": "moves = ['up','up','left','down','right']\nfor m in moves:\n    print('move:', m)",
        "check": lambda out: ('move:' in out) and ('up' in out),
        "success": "Nice list! You iterated over movement steps.",
    },
    "09 • Grid Coordinates": {
        "desc": "Track an (x,y) position while you apply moves. Print final position.",
        "starter": (
            "x, y = 0, 0\n"
            "moves = ['up','up','right','right','down']  # try changing\n"
            "for m in moves:\n"
            "    if m == 'up': y -= 1\n"
            "    elif m == 'down': y += 1\n"
            "    elif m == 'left': x -= 1\n"
            "    elif m == 'right': x += 1\n"
            "print('final:', x, y)\n"
        ),
        "check": lambda out: "final:" in out,
        "success": "Coordinates updated! You can track position now.",
    },
    "10 • Turtle Square (Visual)": {
        "desc": "Use turtle to draw a square. First visual step toward a moving object.",
        "starter": (
            "import turtle as t\n"
            "t.speed(3)\n"
            "for _ in range(4):\n"
            "    t.forward(100)\n"
            "    t.left(90)\n"
            "t.done()\n"
        ),
        "check": lambda out: True,  # visual
        "success": "A window should open with your square 🐢",
    },
    "11 • Dictionaries (Key→Action)": {
        "desc": "Map key names to movement words using a dictionary and print actions.",
        "starter": (
            "keys = ['W','A','S','D','W']\n"
            "keymap = {'W':'up','A':'left','S':'down','D':'right'}\n"
            "for k in keys:\n"
            " print('do:', keymap.get(k,'?'))\n"
        ),
        "check": lambda out: ('do:' in out) and ('up' in out) and ('right' in out),
        "success": "Command map ready!",
    },
    "12 • Nested Loops (Grid Print)": {
        "desc": "Print a 3x3 grid of (r,c) positions.",
        "starter": (
            "for r in range(3):\n"
            " line = ''\n"
            " for c in range(3):\n"
            " line += f'({r},{c}) ' # try changing size\n"
            " print(line)\n"
        ),
        "check": lambda out: "(0,0)" in out and "(2,2)" in out,
        "success": "Grid printed!",
    },
    "13 • Input Guard (Clamp)": {
        "desc": "Pretend we got a speed value; clamp it between 0 and 10 then print it.",
        "starter": (
            "speed = 14 # pretend this came from a slider\n"
            "speed = max(0, min(10, speed))\n"
            "print('speed:', speed)\n"
        ),
        "check": lambda out: "speed:" in out,
        "success": "Safe speed!",
    },
    "14 • Random Route": {
        "desc": "Create 5 random moves from ['up','down','left','right'] and print them.",
        "starter": (
            "import random\n"
            "moves = [random.choice(['up','down','left','right']) for _ in range(5)]\n"
            "print('moves:', moves)\n"
        ),
        "check": lambda out: 'moves:' in out,
        "success": "Randomized route created!",
    },
    "15 • Classes (Robot)": {
        "desc": "Make a tiny Robot class that can move() and report position.",
        "starter": (
            "class Robot:\n"
            "    def __init__(self):\n"
            "        self.x = 0; self.y = 0\n"
            "    def move(self, m):\n"
            "        if m=='up': self.y -= 1\n"
            "        elif m=='down': self.y += 1\n"
            "        elif m=='left': self.x -= 1\n"
            "        elif m=='right': self.x += 1\n"
            "    def pos(self):\n"
            "        return (self.x, self.y)\n\n"
            "r = Robot()\nfor m in ['up','right','right']:\n    r.move(m)\nprint('pos:', r.pos())\n"
        ),
        "check": lambda out: 'pos:' in out,
        "success": "Classy! Your robot can move.",
    },
    "16 • Turtle Triangle (Visual)": {
        "desc": "Draw a triangle with turtle (practice angles & sides).",
        "starter": (
            "import turtle as t\n"
            "t.speed(4)\n"
            "for _ in range(3):\n"
            "    t.forward(120)\n"
            "    t.left(120)\n"
            "t.done()\n"
        ),
        "check": lambda out: True,
        "success": "Triangle drawn!",
    },
    "17 • Path Replay (Turtle)": {
        "desc": "Use a list of moves and distances to draw a path with turtle.",
        "starter": (
            "import turtle as t\n"
            "t.speed(5)\n"
            "path = [('forward',80),('left',90),('forward',80),('right',90),('forward',80)]\n"
            "for action, value in path:\n"
            "    if action=='forward': t.forward(value)\n"
            "    elif action=='left': t.left(value)\n"
            "    elif action=='right': t.right(value)\n"
            "t.done()\n"
        ),
        "check": lambda out: True,
        "success": "Path replayed!",
    },
    "18 • Functions with Params": {
        "desc": "Write move(x,y,dir) → (nx,ny) and use it in a loop.",
        "starter": (
            "def move(x,y,dir):\n"
            "    if dir=='up': return x, y-1\n"
            "    if dir=='down': return x, y+1\n"
            "    if dir=='left': return x-1, y\n"
            "    if dir=='right': return x+1, y\n"
            "    return x,y\n\n"
            "x,y = 0,0\nfor d in ['up','up','right','down']:\n    x,y = move(x,y,d)\nprint('final:', x,y)\n"
        ),
        "check": lambda out: 'final:' in out,
        "success": "Reusable movement achieved!",
    },
    "19 • Obstacles & Bounds": {
        "desc": "Keep the robot inside a 5x5 grid and avoid a rock at (2,2).",
        "starter": (
            "x,y = 0,0\nrock = (2,2)\nW=H=5\nfor d in ['right','right','down','down','down']:\n    nx,ny = x,y\n    if d=='up': ny -= 1\n    elif d=='down': ny += 1\n    elif d=='left': nx -= 1\n    elif d=='right': nx += 1\n    if 0 <= nx < W and 0 <= ny < H and (nx,ny)!=rock:\n        x,y = nx,ny\nprint('safe at:', x,y)\n"
        ),
        "check": lambda out: 'safe at:' in out,
        "success": "Navigation with rules—nice!",
    },
    "20 • Save & Load (JSON)": {
        "desc": "Write a tiny save file with a score and load it back.",
        "starter": (
            "import json, os\n"
            "path = os.path.join('.', 'tiny_save.json')\n"
            "data = {'score': 123}\n"
            "with open(path,'w') as f: f.write(__import__('json').dumps(data))\n"
            "with open(path,'r') as f: back = __import__('json').loads(f.read())\n"
            "print('loaded score =', back['score'])\n"
        ),
        "check": lambda out: 'loaded score' in out,
        "success": "Saved and loaded! Your data round-tripped.",
    },
}
TESTS = [
    {
        "title": "T1 • Print",
        "desc": "Print exactly: Hello, Tester!",
        "starter": "# Print exactly this phrase\nprint('Hello, Tester!')\n",
        "check": lambda out: out.strip() == "Hello, Tester!",
    },
    {
        "title": "T2 • If/Else",
        "desc": "If n is even print EVEN else ODD.",
        "starter": "n = 6  # try other numbers\nif n % 2 == 0:\n    print('EVEN')\nelse:\n    print('ODD')\n",
        "check": lambda out: ("EVEN" in out) or ("ODD" in out),
    },
    {
        "title": "T3 • Loop Sum",
        "desc": "Sum the numbers 1..5 and print total: 15",
        "starter": "total = 0\nfor i in range(1,6):\n    total += i\nprint('total:', total)\n",
        "check": lambda out: "total:" in out and ("15" in out),
    },
    {
        "title": "T4 • Function",
        "desc": "Write shout(x) that returns x.upper() + '!'; print shout('go').",
        "starter": "def shout(x):\n    return x.upper() + '!'\n\nprint(shout('go'))\n",
        "check": lambda out: "GO!" in out,
    },
    {
        "title": "T5 • List & For",
        "desc": "Given moves, print the count of 'up'. Expect: ups: 2",
        "starter": "moves = ['up','down','up','left']\nprint('ups:', moves.count('up'))\n",
        "check": lambda out: "ups:" in out and ("2" in out),
    },
]

# Windows + Canvas
root = tk.Tk()
root.title("Coding Desk")
root.state("zoomed")  # full screen windowed

canvas = tk.Canvas(root, highlightthickness=0)
canvas.pack(fill="both", expand=True)

# Load background once
if not os.path.exists(BG_PATH):
    raise FileNotFoundError(f"Missing background image: {BG_PATH}")
bg_orig = Image.open(BG_PATH).convert("RGBA")
bg_w, bg_h = bg_orig.size

# Clickable hitboxes (x1,y1,x2,y2,callback)
card_hitboxes = []

# Popups: Sandbox / Lessons / Calendar 
def open_sandbox():
    win = tk.Toplevel(root)
    win.title("Sandbox")
    win.geometry("960x640")

    tk.Label(win, text="Sandbox", font=("Segoe UI", 22, "bold")).pack(pady=(8, 0))
    editor = tk.Text(win, font=("Consolas", 14), height=16)
    editor.pack(fill="both", expand=True, padx=12, pady=8)
    editor.insert("1.0", "# Type Python here and press Run\nprint('Hello from Sandbox!')")

    tk.Label(win, text="Output", font=("Segoe UI", 14, "bold")).pack(anchor="w", padx=12)
    output = tk.Text(win, font=("Consolas", 13), height=10, bg="#0b0f14", fg="#e6edf3")
    output.pack(fill="both", expand=False, padx=12, pady=(2,0))

    def run_code():
        code = editor.get("1.0", "end-1c")
        buf_out, buf_err = io.StringIO(), io.StringIO()
        ns = {}
        try:
            old_out, old_err = sys.stdout, sys.stderr
            sys.stdout, sys.stderr = buf_out, buf_err
            exec(code, ns, ns)
        except Exception as e:
            print(f"[error] {e}", file=buf_err)
        finally:
            sys.stdout, sys.stderr = old_out, old_err
        output.delete("1.0", "end")
        output.insert("1.0", buf_out.getvalue() + (("\n" + buf_err.getvalue()) if buf_err.getvalue() else ""))

    row = tk.Frame(win); row.pack(pady=(0, 8))
    tk.Button(row, text="Run ▶", font=("Segoe UI", 12, "bold"), command=run_code).grid(row=10, column=0, padx=6)
    tk.Button(row, text="Clear", font=("Segoe UI", 12), command=lambda: output.delete("1.0","end")).grid(row=10, column=1, padx=6)

def open_lesson(lesson_key):
    global learning
    learning = True
    L = LESSONS[lesson_key]
    win = tk.Toplevel(root)
    win.title(lesson_key)
    win.geometry("960x640")

    tk.Label(win, text=lesson_key, font=("Segoe UI", 22, "bold")).pack(pady=(8, 0))
    tk.Label(win, text=L["desc"], font=("Segoe UI", 13), wraplength=900, justify="left").pack(padx=12)

    editor = tk.Text(win, font=("Consolas", 14), height=16)
    editor.pack(fill="both", expand=True, padx=12, pady=8)
    starter_code = L["starter"] if not HIDE_ANSWERS else L.get("scaffold", "Code here")
    editor.insert("1.0", starter_code)


    tk.Label(win, text="Output", font=("Segoe UI", 14, "bold")).pack(anchor="w", padx=12)
    output = tk.Text(win, font=("Consolas", 13), height=8, bg="#0b0f14", fg="#e6edf3")
    output.pack(fill="both", expand=False, padx=12, pady=(0, 12))

    def run_check():
        code = editor.get("1.0", "end-1c")
        buf_out, buf_err = io.StringIO(), io.StringIO()
        ns = {}
        try:
            old_out, old_err = sys.stdout, sys.stderr
            sys.stdout, sys.stderr = buf_out, buf_err
            exec(code, ns, ns)
        except Exception as e:
            print(f"[error] {e}", file=buf_err)
        finally:
            sys.stdout, sys.stderr = old_out, old_err

        out, err = buf_out.getvalue(), buf_err.getvalue()
        output.delete("1.0", "end")
        output.insert("1.0", out + (("\n" + err) if err else ""))

        try:
            if L["check"](out):
                messagebox.showinfo("Great job!", L["success"])
        except Exception:
            pass

    row = tk.Frame(win); row.pack(pady=(0, 8))
    tk.Button(row, text="Run ▶", font=("Segoe UI", 12, "bold"), command=run_check).grid(row=0, column=0, padx=6)
    tk.Button(row, text="Reset", font=("Segoe UI", 12),
          command=lambda: (editor.delete("1.0","end"),
                           editor.insert("1.0", L["starter"] if not HIDE_ANSWERS else L.get("scaffold", "# TODO")),
                           output.delete("1.0","end"))
          ).grid(row=0, column=1, padx=6)

def open_lessons():
    
    win = tk.Toplevel(root)
    win.title("Lessons")
    win.geometry("760x600")

    tk.Label(win, text="Start here!The basics", font=("Segoe UI", 20)).pack(pady=(10, 6))

    # Scrollable list
    outer = tk.Frame(win); outer.pack(fill="both", expand=True, padx=10, pady=10)
    c = tk.Canvas(outer, highlightthickness=0)
    s = tk.Scrollbar(outer, orient="vertical", command=c.yview)
    list_frame = tk.Frame(c)
    list_frame.bind("<Configure>", lambda e: c.configure(scrollregion=c.bbox("all")))
    c.create_window((0, 0), window=list_frame, anchor="nw")
    c.configure(yscrollcommand=s.set)
    c.pack(side="left", fill="both", expand=True)
    s.pack(side="right", fill="y")

    for name in sorted(LESSONS.keys()):
        row = tk.Frame(list_frame); row.pack(fill="x", expand=True, pady=6)
        tk.Label(row, text=name, font=("Segoe UI", 20, "bold")).pack(anchor="w")
        tk.Label(row, text=LESSONS[name]["desc"], font=("Segoe UI", 12), wraplength=680).pack(anchor="w")
        tk.Button(row, text="Open", font=("Segoe UI", 11, "bold"),
                  command=lambda n=name: open_lesson(n)).pack(anchor="e", pady=(2, 0))

def open_calendar():
    win = tk.Toplevel(root)
    win.title("Calendar")
    win.geometry("640x560")

    cur = {"y": date.today().year, "m": date.today().month}
    notes = {}
    header = tk.Frame(win); header.pack(fill="x", pady=(8,4))
    btn_prev = tk.Button(header, text="◀", width=3)
    btn_next = tk.Button(header, text="▶", width=3)
    lbl = tk.Label(header, font=("Segoe UI", 16, "bold"))
    btn_prev.pack(side="left", padx=8); btn_next.pack(side="right", padx=8); lbl.pack(side="top", pady=2)
    grid = tk.Frame(win); grid.pack(expand=True, fill="both", padx=10, pady=10)

    def rebuild():
        for wdg in grid.winfo_children(): wdg.destroy()
        lbl.config(text=f"{calendar.month_name[cur['m']]}  {cur['y']}")
        for i, name in enumerate(["Mon","Tue","Wed","Thu","Fri","Sat","Sun"]):
            tk.Label(grid, text=name, font=("Segoe UI", 11, "bold")).grid(row=0, column=i, padx=4, pady=4)
        cal = calendar.Calendar(firstweekday=0)
        r = 1
        for week in cal.monthdatescalendar(cur["y"], cur["m"]):
            for c, d in enumerate(week):
                show_note = " •" if d in notes and notes[d] else ""
                btn = tk.Button(grid, text=str(d.day)+show_note, width=6, height=2,
                                relief="ridge",
                                state=("normal" if d.month == cur["m"] else "disabled"))
                btn.grid(row=r, column=c, padx=3, pady=3, sticky="nsew")
                def on_day(dd=d):
                    top = tk.Toplevel(win)
                    top.title(dd.isoformat())
                    tk.Label(top, text=f"{dd:%A, %B %d}", font=("Segoe UI", 14, "bold")).pack(pady=(10,4))
                    ent = tk.Text(top, width=40, height=6); ent.pack(padx=10, pady=10)
                    if dd in notes: ent.insert("1.0", notes[dd])
                    def save_note():
                        notes[dd] = ent.get("1.0","end-1c").strip()
                        top.destroy(); rebuild()
                    tk.Button(top, text="Save", command=save_note).pack(pady=(0,10))
                btn.configure(command=on_day)
            r += 1
        for i in range(7): grid.grid_columnconfigure(i, weight=1)
        for i in range(r+1): grid.grid_rowconfigure(i, weight=1)

    def prev_month():
        if cur["m"] == 1: cur["m"] = 12; cur["y"] -= 1
        else: cur["m"] -= 1
        rebuild()
    def next_month():
        if cur["m"] == 12: cur["m"] = 1; cur["y"] += 1
        else: cur["m"] += 1
        rebuild()
    btn_prev.config(command=prev_month); btn_next.config(command=next_month)
    rebuild()

#Draw / Scale Scene (your logic preserved) 
def draw_scene(event=None):
    card_hitboxes.clear()
    w, h = root.winfo_width(), root.winfo_height()
    if w <= 1 or h <= 1:
        return  # skip initial 0-size events

    # background scaling
    scale = max(w / bg_w, h / bg_h)
    new_size = (max(1, int(bg_w * scale)), max(1, int(bg_h * scale)))
    bg_scaled = bg_orig.resize(new_size, Image.LANCZOS)

    # center
    x = (w - new_size[0]) // 2
    y = (h - new_size[1]) // 2

    # draw bg
    photo = ImageTk.PhotoImage(bg_scaled)
    canvas.photo_bg = photo
    canvas.delete("all")
    canvas.create_image(x, y, image=photo, anchor="nw")

    # UI scale from 2560x1440
    ui_scale = min(w / BASE_W, h / BASE_H)
    S = lambda px: int(px * ui_scale)

    # Title
    canvas.create_text(S(180), S(100), text="Coding Desk",
                       font=("Segoe UI", S(64), "bold"), fill="#1B1F3B", anchor="w")

    # Card art (wide pill)
    if not os.path.exists(CARD_PATH):
        # simple fallback if card image missing
        card_img = Image.new("RGBA", (1200, 180), (130, 160, 255, 200))
        ImageDraw.Draw(card_img).rounded_rectangle((0,0,1199,179), radius=40, fill=(130,160,255,200))
    else:
        card_img = Image.open(CARD_PATH).convert("RGBA")

    card_target = (S(1200), S(180))  # tweak height here if you want larger
    card_scaled = ImageTk.PhotoImage(card_img.resize(card_target, Image.LANCZOS))
    canvas.card_scaled = card_scaled

    # Three menu cards (as requested)
    cards = [
        ("Lessons",   (320, 560), open_lessons),
        ("Calendar",  (320, 760), open_calendar),
        ("Sandbox",   (320, 960), open_sandbox),
    ]

    for label, (cx, cy), cb in cards:
        px, py = S(cx), S(cy)
        canvas.create_image(px, py, image=card_scaled, anchor="nw")
        tx = px + card_target[0] // 2
        ty = py + card_target[1] // 2
        # subtle shadow + text
        canvas.create_text(tx+S(2), ty+S(2), text=label,
                           font=("Segoe UI", S(34), "bold"), fill="#000000", anchor="c")
        canvas.create_text(tx, ty, text=label,
                           font=("Segoe UI", S(34), "bold"), fill="#ffffff", anchor="c")
        # record clickable area
        card_hitboxes.append((px, py, px+card_target[0], py+card_target[1], cb))

    # Mascot (optional)
    if os.path.exists(MASCOT_PATH):
        try:
            m_img = Image.open(MASCOT_PATH).convert("RGBA")
            m_size = (S(420), S(420))
            m_scaled = ImageTk.PhotoImage(m_img.resize(m_size, Image.LANCZOS))
            canvas.mascot_scaled = m_scaled
            canvas.create_image(w - S(420) - S(80), h - S(420) - S(120), image=m_scaled, anchor="nw")
        except Exception:
            pass

def on_click(event):
    for (x1, y1, x2, y2, cb) in card_hitboxes:
        if x1 <= event.x <= x2 and y1 <= event.y <= y2:
            cb(); break

def on_motion(event):
    for (x1, y1, x2, y2, _) in card_hitboxes:
        if x1 <= event.x <= x2 and y1 <= event.y <= y2:
            canvas.config(cursor="hand2"); return
    canvas.config(cursor="")

# ---------------- Voice Flask Server ----------------
flask_app = Flask(__name__)

UPLOAD_FOLDER = "uploads"
os.makedirs(UPLOAD_FOLDER, exist_ok=True)
UPLOAD_PATH = os.path.join(UPLOAD_FOLDER, "recording.wav")

# Add any words you want to react to
KEYWORDS = ["calendar", "question", "weather", "time", "trash"]

@flask_app.route("/", methods=["GET"])
def index():
    return "ESP32 Audio Upload Server Running!"

@flask_app.route("/upload", methods=["POST"])

def upload_audio():
    global questionflag, last_transcript, learning
    audio_data = request.data
    if learning:
        print("LEARNING MODE ON")
        
    elif not audio_data:
        return "No data received", 400

    with open(UPLOAD_PATH, "wb") as f:
        f.write(audio_data)
    print(f"[INFO] File received: {UPLOAD_PATH}")

    recognizer = sr.Recognizer()
    try:
        with sr.AudioFile(UPLOAD_PATH) as source:
            audio = recognizer.record(source)
            text = recognizer.recognize_google(audio)
            print(f"[TRANSCRIPTION]: {text}")

            text_lower = text.lower()

            if questionflag:
                gemini_response(text_lower)
                last_transcript = text
                show_info()
                questionflag = False

            for kw in KEYWORDS:
                if kw in text_lower:
                    print(f"[KEYWORD DETECTED]: {kw}")
                    if kw == "question":
                        questionflag = True
                    voice_queue.put(kw)  # push to Tk
                    return kw, 200

            return "none", 200

    except sr.UnknownValueError:
        return "Could not understand audio", 400
    except sr.RequestError as e:
        return f"SpeechRecognition API error: {e}", 500

def gemini_response(prompt):
    global last_response
    response = client.models.generate_content(
        model="gemini-2.5-flash", contents= "Answer the question in a few words" + prompt
    )
    last_response = response.text
    print(last_response)

def run_flask():
    # threaded=True so multiple uploads don't block; no reloader to avoid extra processes
    flask_app.run(host="0.0.0.0", port=5000, debug=False, use_reloader=False, threaded=True)

# ---------------- Voice → GUI Bridge ----------------
def show_time():
    from datetime import datetime
    messagebox.showinfo("Time", f"Current time: {datetime.now().strftime('%I:%M %p')}")

def show_info():
    global last_transcript, last_response

    answer_text = last_response
    messagebox.showinfo(
        "Question",
        f"You asked: {last_transcript}\n\nAnswer: {answer_text}"
    )


def handle_keyword(kw: str):
    routing = {
        "calendar": open_calendar,
        "question": show_info
    }
    cb = routing.get(kw)
    if cb:
        cb()

def poll_voice_events():
    try:
        while True:
            kw = voice_queue.get_nowait()
            print(f"[GUI] Handling keyword: {kw}")
            handle_keyword(kw)
    except Empty:
        pass
    finally:
        root.after(100, poll_voice_events)  # poll every 100 ms

# Bind / Run 
root.bind("<Configure>", draw_scene)
canvas.bind("<Button-1>", on_click)
canvas.bind("<Motion>", on_motion)

# Start Flask server in the background
server_thread = threading.Thread(target=run_flask, daemon=True)
server_thread.start()
print("[INFO] Flask server thread started on http://0.0.0.0:5000")

# Start polling the queue for detected keywords
voice_queue: Queue[str] = Queue()
poll_voice_events()

# Initial paint + Tk main loop
draw_scene()
root.mainloop()