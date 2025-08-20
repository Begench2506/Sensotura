from flask import Flask, render_template
from sqlalchemy import text, desc, func 
from models import *
from config import DevelopmentConfig
from datetime import datetime, timedelta
import traceback
from collections import defaultdict

app = Flask(__name__)
app.config.from_object(DevelopmentConfig)
db.init_app(app)

@app.route('/')
def index():
    # Сколько последних записей брать по каждому датчику
    records_per_sensor = 12

    # 1) Получаем список датчиков на основе фактических данных и сопоставляем названия из Fridge
    sensor_ids = [row[0] for row in db.session.query(TempData.sensor_id).distinct().all()]
    sensor_id_to_name = {}
    for sid in sensor_ids:
        fridge = Fridge.query.get(sid)
        sensor_id_to_name[sid] = (fridge.name if fridge and fridge.name else f"Датчик {sid}")

    if not sensor_id_to_name:
        # Нет данных — отдаем пустой график
        return render_template("index.html", labels=[], datasets=[])

    # 2) Для каждого датчика берем последние N записей
    sensor_id_to_points = {}
    all_timestamps = set()
    for sensor_id in sensor_id_to_name.keys():
        rows = (
            TempData.query
            .filter(TempData.sensor_id == sensor_id)
            .order_by(TempData.datetime.desc())
            .limit(records_per_sensor)
            .all()
        )
        # Сортируем по возрастанию времени для корректного отображения линии
        rows = list(reversed(rows))

        # Округляем до минут, чтобы унифицировать метки
        points = {}
        for r in rows:
            ts = r.datetime.replace(second=0, microsecond=0)
            points[ts] = float(r.tvalue) if r.tvalue is not None else None
            all_timestamps.add(ts)
        sensor_id_to_points[sensor_id] = points

    # 3) Единая ось X — объединение всех меток времени
    sorted_timestamps = sorted(all_timestamps)
    # Подписи (часы:минуты); при необходимости можно добавить дату
    labels = [dt.strftime('%H:%M') for dt in sorted_timestamps]

    # 4) Готовим datasets для Chart.js, выравнивая по общей оси X
    colors = ['#3b82f6', '#f59e0b', '#ef4444', '#10b981', '#8b5cf6', '#06b6d4', '#84cc16', '#f97316']
    datasets = []
    for i, (sensor_id, name) in enumerate(sensor_id_to_name.items()):
        points = sensor_id_to_points.get(sensor_id, {})
        values = [points.get(ts, None) for ts in sorted_timestamps]
        datasets.append({
            "label": name,
            "data": values,
            "borderColor": colors[i % len(colors)],
            "fill": False,
            "tension": 0.3,
            "pointRadius": 3
        })

    return render_template("index.html", labels=labels, datasets=datasets)

if __name__ == '__main__':
    with app.app_context():
        db.create_all()
    app.run(debug=True)
