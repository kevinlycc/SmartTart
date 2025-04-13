import sqlite3
from datetime import datetime, timedelta
import random
import os

# DB setup
DB_FILE = "smart_tart.db"

def init_db():
    if os.path.exists("smart_tart.db"):
        os.remove("smart_tart.db")
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS toast_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                time TEXT,
                duration INTEGER
            )
        """)
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS calorie_log (
                time TEXT,
                calories INTEGER
            )
        """)
        conn.commit()

# def insert_sample_data():
#     now = datetime.now()

#     with sqlite3.connect(DB_FILE) as conn:
#         cursor = conn.cursor()

#         # Insert sample toast history (5 entries)
#         for i in range(5):
#             time = (now - timedelta(days=i)).isoformat()
#             duration = random.choice([60, 90, 120])
#             cursor.execute("INSERT INTO toast_history (time, duration) VALUES (?, ?)", (time, duration))

#         # Insert sample calorie log (5 entries)
#         for i in range(5):
#             time = (now - timedelta(days=i)).isoformat()
#             # Simulate 2 toasts and 3000 steps
#             calories = 200 * 2 - 27 * 3000 // 1000
#             cursor.execute("INSERT INTO calorie_log (time, calories) VALUES (?, ?)", (time, calories))

#         conn.commit()

init_db()
# insert_sample_data()
