from flask_sqlalchemy import SQLAlchemy
from datetime import datetime

db = SQLAlchemy()

class TempData(db.Model):
    __tablename__ = 'temperature_data'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    sensor_id = db.Column(db.Integer)
    tvalue = db.Column(db.Float)
    datetime = db.Column(db.DateTime, default=datetime.utcnow)