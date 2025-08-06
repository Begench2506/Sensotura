from flask import Flask, render_template, request, redirect, url_for
from sqlalchemy import text
from models import db, TempData
from config import DevelopmentConfig 
from datetime import datetime, timedelta
import os

app = Flask(__name__)

app.config.from_object(DevelopmentConfig)

db.init_app(app)

@app.route('/')
def index():
    return render_template('index.html')


if __name__ == '__main__':
    with app.app_context():
        try:
            db.create_all()
            print("Таблицы успешно созданы")
        except Exception as e:
            print(f"Ошибка при создании таблиц: {e}")
    app.run(debug=True)