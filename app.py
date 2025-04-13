# smart_tart_backend.py
from flask import Flask, request, jsonify, render_template
from datetime import datetime
import sqlite3
import requests

app = Flask(__name__, static_folder="static", template_folder="templates")
DB_FILE = "smart_tart.db"

# Fake internal state (replace with real hardware integrations)
state = {
    "profile": "medium",
    "tarts": 0,
    "steps": 0,
    "current_temp": 25.0
}

# def log_toast(profile, duration):
#     now = datetime.now().isoformat()
#     with sqlite3.connect(DB_FILE) as conn:
#         cursor = conn.cursor()
#         cursor.execute("INSERT INTO toast_history (time, profile, duration) VALUES (?, ?, ?)",
#                        (now, profile, duration))
#         cursor.execute("INSERT INTO calorie_log (timestamp, consumed, spent) VALUES (?, ?, ?)",
#                        (now, 200, 0))  # calories spent will be updated in /stats
#         conn.commit()

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/test_send", methods = ["POST"])
def test():
    data = request.get_json()
    message = data.get("message")

    print(f"Message Received: {message}")

    return jsonify({"status": "Message Received"}), 200


@app.route("/set_profile", methods=["POST"])
def set_profile():
    data = request.get_json()
    profile = data.get("profile")
    print(profile)
    if profile not in ["light", "medium", "crispy"]:
        state["profile"] = "custom"
    else: 
        state["profile"] = profile

    if profile == "light":
        state["duration"] = 30
    elif profile == "medium":
        state["duration"] = 60
    elif profile == "crispy":
        state["duration"] = 90
    else:
        state["duration"] = int(profile)
    
    state["tarts"] += 1
    # log_toast(profile, state["duration"])

    # Send updated profile to the ESP32
    esp32_ip = "http://ESP32_IP_ADDRESS"  # Replace with the ESP32 IP
    payload = {"profile": profile}
    try:
        response = requests.post(f"{esp32_ip}/set_profile", json=payload)
        return jsonify({"status": "success", "profile": profile}), 200
    except requests.exceptions.RequestException as e:
        return jsonify({"error": "ESP32 not reachable"}), 400


@app.route("/stats")
def stats():
    calories_spent = state["steps"] * 27
    now = datetime.now().isoformat()

    # with sqlite3.connect(DB_FILE) as conn:
    #     cursor = conn.cursor()
    #     cursor.execute("INSERT INTO calorie_log (timestamp, consumed, spent) VALUES (?, ?, ?)",
    #                    (now, 0, calories_spent))
    #     conn.commit()

    return jsonify({
        "tarts": state["tarts"],
        "steps": state["steps"],
        "current_temp": round(state["current_temp"], 1),
    })

# @app.route("/history")
# def history():
#     with sqlite3.connect(DB_FILE) as conn:
#         cursor = conn.cursor()
#         cursor.execute("SELECT time, profile, duration FROM toast_history ORDER BY time DESC")
#         rows = cursor.fetchall()
#         return jsonify([
#             {"time": row[0], "profile": row[1], "duration": row[2]}
#             for row in rows
#         ])
    
# @app.route("/calories_graph")
# def calories_graph():
#     with sqlite3.connect(DB_FILE) as conn:
#         cursor = conn.cursor()
#         cursor.execute("SELECT timestamp, SUM(consumed), SUM(spent) FROM calorie_log GROUP BY timestamp ORDER BY timestamp")
#         rows = cursor.fetchall()
#         return jsonify([
#             {"time": row[0], "consumed": row[1], "spent": row[2]}
#             for row in rows
#         ])


@app.route("/update_steps", methods=["POST"])
def update_steps():
    data = request.get_json()
    steps = data.get("steps", 0)
    if isinstance(steps, int) and steps > 0:
        state["steps"] += steps
        return jsonify({"status": "steps updated"}), 200
    return jsonify({"error": "invalid step data"}), 400

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)