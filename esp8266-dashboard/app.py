from flask import Flask, render_template_string, request, jsonify
import sqlite3
from datetime import datetime

app = Flask(__name__)
DB_NAME = "readings.db"

def init_db():
  """Create SQLite database and table if they don't exist."""
  with sqlite3.connect(DB_NAME) as conn:
    cursor = conn.cursor()
    cursor.execute('''
      CREATE TABLE IF NOT EXISTS measurements (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
        indoor REAL,
        outdoor REAL,
        delta_t REAL,
        wind REAL
      )
    ''')
    conn.commit()

init_db()

@app.route('/')
def index():
  """Serve the Web Dashboard,"""
  with open("index.html", "r", encoding="utf-8") as f:
    return render_template_string(f.read())


@app.route('/api/reading', methods=['POST'])
def receive_reading():
  """API Endpoint for ESP8266 to POST sensor data."""
  data = request.get_json() or request.form

  try:
    indoor = float(data.get('indoor'))
    outdoor = float(data.get('outdoor'))
    delta_t = float(data.get('delta_t'))
    wind = float(data.get('wind', 0.0))

    with sqlite3.connect(DB_NAME) as conn:
      cursor = conn.cursor()
      cursor.execute('''
        INSERT INTO measurements (indoor, outdoor, delta_t, wind)
        VALUES (?, ?, ?, ?)
      ''', (indoor, outdoor, delta_t, wind))
      conn.commit()

      return jsonify({"status": "success"}), 201
  except (TypeError, ValueError) as e:
    return jsonify({"error": f"Invalid data payload: {str(e)}"}), 400


@app.route('/api/data', methods=['GET'])
def get_data():
    """API Endpoint for Chart.js to fetch historic readings."""
    limit = request.args.get('limit', 50, type=int)
    
    with sqlite3.connect(DB_NAME) as conn:
        cursor = conn.cursor()
        cursor.execute('''
            SELECT timestamp, indoor, outdoor, delta_t, wind 
            FROM measurements 
            ORDER BY id DESC LIMIT ?
        ''', (limit,))
        rows = cursor.fetchall()
        
    # Reverse rows so they are in chronological order
    rows.reverse()
    
    return jsonify({
        "timestamps": [r[0] for r in rows],
        "indoor": [r[1] for r in rows],
        "outdoor": [r[2] for r in rows],
        "delta_t": [r[3] for r in rows],
        "wind": [r[4] for r in rows]
    })

if __name__ == '__main__':
    # Listens on all interfaces so ESP8266 can connect over LAN
    app.run(host='0.0.0.0', port=5000, debug=True)
