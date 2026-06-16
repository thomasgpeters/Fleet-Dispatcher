FROM python:3.12-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app ./app

# ANTHROPIC_API_KEY, HERE_API_KEY, FLEET_API_BASE_URL, etc. injected at runtime.
ENV ASSISTANT_PORT=5710
EXPOSE 5710

CMD ["sh", "-c", "uvicorn app.main:app --host 0.0.0.0 --port ${ASSISTANT_PORT:-5710}"]
