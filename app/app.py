from flask import Flask, render_template
from sqlalchemy import text, desc
from models import db, TempData
from config import DevelopmentConfig
from datetime import datetime, timedelta
import traceback

app = Flask(__name__)
app.config.from_object(DevelopmentConfig)
db.init_app(app)

@app.route('/')
def index():
    """Главная – 24 последних измерения"""
    # Берём 24 самых свежих записи и разворачиваем в хронологический порядок
    recent = (TempData.query.order_by(desc(TempData.datetime)).limit(12).all()[::-1])
    
    chart_data = [
        {
            "ts": row.datetime.strftime('%H:%M'),  # подпись на оси X
            "temp": row.tvalue
        } for row in recent
    ]
    return render_template('index.html', chart_data=chart_data)


if __name__ == '__main__':
    with app.app_context():
        db.create_all()
    app.run(debug=True)
