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
    now = datetime.utcnow()
    hours_back = 12

    # Список временных меток (datetime) по часам
    hour_labels = [
        (now - timedelta(hours=i)).replace(minute=0, second=0, microsecond=0)
        for i in reversed(range(hours_back))
    ]
    labels = [dt.strftime('%H:%M') for dt in hour_labels]

    # Получаем все холодильники
    fridges = Fridge.query.all()

    # Цвета для графика
    colors = ['#3b82f6', '#f59e0b', '#ef4444', '#10b981', '#8b5cf6']

    datasets = []

    for i, fridge in enumerate(fridges):
        # Группируем значения по часу (MySQL)
        rows = (
            db.session.query(
                func.date_format(TempData.datetime, '%Y-%m-%d %H:00:00'),
                func.avg(TempData.tvalue)
            )
            .filter(
                TempData.sensor_id == fridge.id,
                TempData.datetime >= hour_labels[0],
                TempData.datetime <= hour_labels[-1]
            )
            .group_by(func.date_format(TempData.datetime, '%Y-%m-%d %H:00:00'))
            .all()
        )

        # Преобразуем в словарь {час: значение}
        data_by_hour = {datetime.strptime(row[0], '%Y-%m-%d %H:%M:%S'): round(row[1], 2) for row in rows}

        # Список температур по каждому часу (или None, если пропущено)
        temp_values = [data_by_hour.get(h, None) for h in hour_labels]

        datasets.append({
            "label": fridge.name or f"Холодильник {fridge.id}",
            "data": temp_values,
            "borderColor": colors[i % len(colors)],
            "fill": False,
            "tension": 0.3,
            "pointRadius": 3
        })

     

    return render_template("index.html", labels=labels, datasets=datasets)
    print("Диапазон дат:", hour_labels[0], "→", hour_labels[-1]) 

if __name__ == '__main__':
    with app.app_context():
        db.create_all()
    app.run(debug=True)
