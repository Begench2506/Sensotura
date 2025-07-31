import os
from dotenv import load_dotenv

load_dotenv()  # Загружает переменные из .env

class Config:
    SECRET_KEY = os.getenv('SECRET_KEY')
    SQLALCHEMY_TRACK_MODIFICATIONS = False
    SQLALCHEMY_ECHO = False  # По умолчанию False (только для разработки)

    # Динамическое формирование URI
    SQLALCHEMY_DATABASE_URI = (
        f"mysql+mysqlconnector://"
        f"{os.getenv('DB_USER')}:{os.getenv('DB_PASSWORD')}"
        f"@{os.getenv('DB_HOST')}/{os.getenv('DB_NAME')}"
    )

class DevelopmentConfig(Config):
    SQLALCHEMY_ECHO = True  # Показывать SQL-запросы в консоли

class ProductionConfig(Config):
    SQLALCHEMY_ECHO = False