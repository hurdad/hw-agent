FROM python:3.11-slim
WORKDIR /app
COPY pyproject.toml README.md /app/
COPY src /app/src
RUN pip install --no-cache-dir .
ENTRYPOINT ["python", "-m", "hw_agent_tui"]
