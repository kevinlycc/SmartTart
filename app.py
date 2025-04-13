# smart_tart_backend.py
from flask import Flask, request, jsonify, render_template
from datetime import datetime
import sqlite3
import requests

app = Flask(__name__, static_folder="static", template_folder="templates")
DB_FILE = "smart_tart.db"

# Fake internal state (replace with real hardware integrations)
TARTS = 0
STEPS = 0
CURRENT_TEMP = 0
CALORIES_SPENT = 0
CALORIES_CONSUMED = 0

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/set_profile", methods=["POST"])
def set_profile():
    data = request.get_json()
    profile = data.get("profile")

    if profile == "light":
        duration = 60
    elif profile == "medium":
        duration = 120
    elif profile == "crispy":
        duration = 180
    else:
        duration = int(profile)
    
    global TARTS
    TARTS += 1

    now = datetime.now().isoformat()
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute("INSERT INTO toast_history (time, duration) VALUES (?, ?)",
                       (now, duration))
        conn.commit()

    # Send updated profile to the ESP32
    esp32_ip = "http://192.168.53.49"  # Replace with the ESP32 IP
    payload = {"duration": duration}
    try:
        print("Sending message...")
        response = requests.post(f"{esp32_ip}/set_profile", json=payload)
        print("MESSAGE SENT")
        return jsonify({"status": "success", "profile": profile}), 200
    except requests.exceptions.RequestException as e:
        return jsonify({"error": "ESP32 not reachable", "message": f"{e}",}), 400

@app.route("/stats", methods = ["POST"])
def stats():
    data = request.get_json()
    tarts = data.get("tarts")
    steps = data.get("steps")
    current_temp = data.get("current_temp")

    global TARTS, STEPS, CALORIES_CONSUMED, CALORIES_SPENT, CURRENT_TEMP
    TARTS = tarts if tarts > TARTS else TARTS
    STEPS = steps
    CALORIES_SPENT = int(STEPS / 27)
    CALORIES_CONSUMED = TARTS * 200
    CURRENT_TEMP = current_temp

    net_cals = CALORIES_CONSUMED - CALORIES_SPENT
    now = datetime.now().isoformat()
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute("INSERT INTO calorie_log (time, calories) VALUES (?, ?)",
                       (now, net_cals))
        conn.commit()

    return jsonify({
        "tarts": TARTS,
        "steps": STEPS,
        "current_temp": round(CURRENT_TEMP, 1),
    })

@app.route("/stats_vals")
def stats_vals():
    global TARTS, STEPS, CALORIES_CONSUMED, CALORIES_SPENT, CURRENT_TEMP
    return jsonify({
        "tarts": TARTS,
        "steps": STEPS,
        "calories_consumed": CALORIES_CONSUMED,
        "calories_spent": CALORIES_SPENT,
        "current_temp": round(CURRENT_TEMP, 1),
    }) 

@app.route("/history")
def history():
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT time, duration FROM toast_history ORDER BY time DESC")
        rows = cursor.fetchall()
        return jsonify([
            {"time": row[0], "duration": row[1]}
            for row in rows
        ])
    
@app.route("/calories_graph")
def calories_graph():
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute("SELECT time, calories FROM calorie_log GROUP BY time ORDER BY time")
        rows = cursor.fetchall()
        return jsonify([
            {"time": row[0], "calories": row[1]}
            for row in rows
        ])

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)