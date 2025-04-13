import sqlite3

# DB setup
DB_FILE = "smart_tart.db"

def init_db():
    with sqlite3.connect(DB_FILE) as conn:
        cursor = conn.cursor()
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS toast_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                time TEXT,
                profile TEXT,
                duration INTEGER
            )
        """)
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS calorie_log (
                timestamp TEXT,
                consumed INTEGER,
                spent INTEGER
            )
        """)
        conn.commit()

init_db()
