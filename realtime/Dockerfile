# Fleet Dispatcher — realtime WebSocket bridge (Kafka -> WebSocket).
FROM python:3.12-slim

WORKDIR /srv/realtime

# confluent-kafka ships manylinux wheels bundling librdkafka, so no apt build deps.
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app ./app

# WebSocket port (override with BRIDGE_PORT).
EXPOSE 8765

CMD ["python", "-m", "app.main"]
