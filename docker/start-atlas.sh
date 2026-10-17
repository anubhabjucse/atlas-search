
#!/usr/bin/env bash
set -Eeuo pipefail
export LD_LIBRARY_PATH="/app${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
MODEL_PATH="/models/embeddinggemma-300M-Q8_0.gguf"

echo "Starting EmbeddingGemma..."
/app/llama-server \
  -m "$MODEL_PATH" \
  --embedding \
  --host 127.0.0.1 \
  --port 8080 &
EMBEDDING_PID=$!

echo "Waiting for embedding server..."
for i in $(seq 1 60); do
  if ! kill -0 "$EMBEDDING_PID" 2>/dev/null; then
    echo "Embedding server exited during startup."
    wait "$EMBEDDING_PID" || true
    exit 1
  fi

  if curl -fsS http://127.0.0.1:8080/health >/dev/null 2>&1; then
    break
  fi

  sleep 2
done

if ! curl -fsS http://127.0.0.1:8080/health >/dev/null 2>&1; then
  echo "Embedding server did not become healthy in time."
  kill "$EMBEDDING_PID" 2>/dev/null || true
  exit 1
fi

echo "Embedding server is ready."
echo "Starting Atlas API..."

cd /atlas/api/backend
npm start &
API_PID=$!

cleanup() {
  echo "Stopping Atlas processes..."
  kill "$API_PID" "$EMBEDDING_PID" 2>/dev/null || true
  wait "$API_PID" 2>/dev/null || true
  wait "$EMBEDDING_PID" 2>/dev/null || true
}

trap cleanup SIGINT SIGTERM EXIT

# If either process exits, stop the container rather than leave
# a partially functioning search service running.
set +e
wait -n "$EMBEDDING_PID" "$API_PID"
STATUS=$?
set -e

echo "A required Atlas process exited (status $STATUS)."
exit "$STATUS"
