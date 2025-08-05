import os
import urllib.parse
from dotenv import load_dotenv
from pathlib import Path

basedir = Path(__file__).resolve().parent.parent
load_dotenv(dotenv_path=basedir / '.env')

print("SECRET_KEY from .env:", os.getenv("SECRET_KEY"))

class Config:
    SECRET_KEY = os.getenv('SECRET_KEY')
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    SQLALCHEMY_ECHO = False

    DB_USER = os.getenv('DB_USER')
    DB_PASSWORD = urllib.parse.quote_plus(os.getenv('DB_PASSWORD'))
    DB_HOST = os.getenv('DB_HOST')
    DB_NAME = os.getenv('DB_NAME')

    SQLALCHEMY_DATABASE_URI = (
        f"mysql+mysqlconnector://{DB_USER}:{DB_PASSWORD}@{DB_HOST}/{DB_NAME}"
    )

    print("SQLAlchemy URI:", SQLALCHEMY_DATABASE_URI)

class DevelopmentConfig(Config):
    SQLALCHEMY_ECHO = True

class ProductionConfig(Config):
    SQLALCHEMY_ECHO = False
