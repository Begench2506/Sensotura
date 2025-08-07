import os
from pathlib import Path
from urllib.parse import quote_plus
from dotenv import load_dotenv

# Получаем путь до .env (на уровень выше /app)
basedir = Path(__file__).resolve().parent.parent
load_dotenv(dotenv_path=basedir / '.env')

# Загружаем переменные окружения
DB_USER = os.getenv('DB_USER')
DB_PASSWORD = os.getenv('DB_PASSWORD')
DB_HOST = os.getenv('DB_HOST')
DB_PORT = os.getenv('DB_PORT') or '3306'
DB_NAME = os.getenv('DB_NAME')
SECRET_KEY = os.getenv('SECRET_KEY')

SQLALCHEMY_DATABASE_URI = (
    f"mysql+pymysql://{DB_USER}:{DB_PASSWORD}@{DB_HOST}:{DB_PORT}/{DB_NAME}"
)


class Config:
    SECRET_KEY = SECRET_KEY
    SQLALCHEMY_DATABASE_URI = SQLALCHEMY_DATABASE_URI
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    SQLALCHEMY_ECHO = False  


class DevelopmentConfig(Config):
    SQLALCHEMY_ECHO = True

class ProductionConfig(Config):
    SQLALCHEMY_ECHO = False
