from flask_sqlalchemy import SQLAlchemy
from datetime import datetime

db = SQLAlchemy()

class TempData(db.Model):
    __tablename__ = 'temperature_data'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    sensor_id = db.Column(db.Integer, db.ForeignKey('fridges.id'), nullable=False)
    tvalue = db.Column(db.Float)
    datetime = db.Column(db.DateTime, default=datetime.utcnow)


class Fridge(db.Model):
    __tablename__ = 'fridges'

    id = db.Column(db.Integer, primary_key=True, autoincrement=True)
    name = db.Column(db.String(50), nullable=False)

    temps = db.relationship("TempData", backref="fridge", lazy=True)
    
    def __repr__(self):
        return f"<Fridge(id={self.id}, name='{self.name}')>"