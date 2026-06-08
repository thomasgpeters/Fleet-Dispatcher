FROM python:3.12-slim
WORKDIR /srv

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app ./app

ENV GIS_PORT=5701
EXPOSE 5701
CMD ["sh", "-c", "uvicorn app.main:app --host 0.0.0.0 --port ${GIS_PORT:-5701}"]
