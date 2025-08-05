from flask import Flask, render_template, request, redirect, url_for
from sqlalchemy import text
from models import db, TempData
from config import DevelopmentConfig 
import os

app = Flask(__name__)

app.config.from_object(DevelopmentConfig)

db.init_app(app)

@app.route('/')
def index():
    try:
        with app.app_context():
            db.session.execute(text("SELECT 1"))
            print("Подключение к БД успешно!")
            data = TempData.query.limit(10).all()
            return render_template('index.html', data=data)
    except Exception as e:
        print(f"Ошибка БД: {str(e)}")
        return render_template('index.html', data=None)


if __name__ == '__main__':
    with app.app_context():
        try:
            db.create_all()
            print("Таблицы успешно созданы")
        except Exception as e:
            print(f"Ошибка при создании таблиц: {e}")
    app.run(debug=True)