from flask import Flask, render_template
from sqlalchemy import desc
from models import *
from config import DevelopmentConfig
from datetime import datetime
from collections import defaultdict

app = Flask(__name__)
app.config.from_object(DevelopmentConfig)
db.init_app(app)

@app.route('/')
def index():
    TOTAL_POINTS = 12

    # 1. Получаем последние 12 уникальных временных меток из всей таблицы
    timestamps_query = (
        db.session.query(TempData.datetime)
        .order_by(desc(TempData.datetime))
    )

    seen = set()
    timestamps = []
    for row in timestamps_query:
        dt = row.datetime.replace(second=0, microsecond=0)
        if dt not in seen:
            seen.add(dt)
            timestamps.append(dt)
        if len(timestamps) == TOTAL_POINTS:
            break

    timestamps = sorted(timestamps)  # хронологически

    if not timestamps:
        return render_template("index.html", labels=[], datasets=[], fridges=[])

    labels = [ts.strftime('%H:%M') for ts in timestamps]

    # 2. Загружаем все TempData, попадающие в эти 12 меток
    data = (
        db.session.query(TempData)
        .filter(TempData.datetime.in_(timestamps))
        .all()
    )

    # 3. Группируем по sensor_id
    sensor_data = defaultdict(dict)
    for row in data:
        ts = row.datetime.replace(second=0, microsecond=0)
        sensor_data[row.sensor_id][ts] = float(row.tvalue)

    # 4. Сопоставление sensor_id → name
    sensor_id_to_name = {}
    for sid in sensor_data:
        fridge = Fridge.query.get(sid)
        sensor_id_to_name[sid] = fridge.name if fridge and fridge.name else f"Датчик {sid}"

    # 5. Подготовка datasets
    colors = ['#3b82f6', '#f59e0b', '#ef4444', '#10b981', '#8b5cf6', '#06b6d4']
    datasets = []

    for i, (sid, points) in enumerate(sensor_data.items()):
        values = [points.get(ts, None) for ts in timestamps]
        datasets.append({
            "label": sensor_id_to_name[sid],
            "data": values,
            "borderColor": colors[i % len(colors)],
            "fill": False,
            "tension": 0.3,
            "pointRadius": 5
        })

    # 6. Данные для карточек холодильников
    fridges = Fridge.query.all()
    fridge_data = []

    for fridge in fridges:
        last = (
            TempData.query
            .filter_by(sensor_id=fridge.id)
            .order_by(desc(TempData.datetime))
            .first()
        )
        fridge_data.append({
            "name": fridge.name,
            "last_temp": last.tvalue if last else "Нет данных",
            "last_time": last.datetime.strftime('%H:%M %d.%m') if last else "-"
        })

    return render_template("index.html", labels=labels, datasets=datasets, fridges=fridge_data)

if __name__ == '__main__':
    with app.app_context():
        db.create_all()
    app.run(debug=True)
