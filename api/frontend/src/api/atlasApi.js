// atlasApi.js
//
// The only file that knows about Atlas's HTTP contract. Components never
// call fetch() directly — they call the functions here. That keeps the
// three fixed routes (health, search, documents/:id) in one place, so
// V5 additions (suggestions, semantic search, filters) can be dropped in
// without touching component code.

const JSON_HEADERS = { "Content-Type": "application/json" };

class AtlasApiError extends Error {
  constructor(message, { cause, status } = {}) {
    super(message);
    this.name = "AtlasApiError";
    this.status = status;
    this.cause = cause;
  }
}

async function parseJsonOrThrow(response, fallbackMessage) {
  let body = null;
  try {
    body = await response.json();
  } catch (cause) {
    throw new AtlasApiError(fallbackMessage, { status: response.status, cause });
  }

  if (!response.ok) {
    throw new AtlasApiError(fallbackMessage, { status: response.status });
  }

  return body;
}

/**
 * GET /api/health
 * Used only for a small, non-blocking status indicator. Never gates the
 * search UI: if this call fails, the app still lets the user search.
 */
export async function fetchHealth() {
  const response = await fetch("/api/health");
  return parseJsonOrThrow(response, "Atlas health check failed.");
}

/**
 * POST /api/search
 * @param {string} query
 * @param {number} limit - integer 1-100
 */
export async function runSearch(query, limit = 10) {
  let response;
  try {
    response = await fetch("/api/search", {
      method: "POST",
      headers: JSON_HEADERS,
      body: JSON.stringify({ query, limit }),
    });
  } catch (cause) {
    throw new AtlasApiError(
      "Couldn't reach the Atlas search service.",
      { cause }
    );
  }

  return parseJsonOrThrow(response, "Atlas couldn't complete that search.");
}

/**
 * GET /api/documents/:id
 * @param {string|number} documentId
 */
export async function fetchDocument(documentId) {
  let response;
  try {
    response = await fetch(`/api/documents/${encodeURIComponent(documentId)}`);
  } catch (cause) {
    throw new AtlasApiError(
      "Couldn't reach Atlas to load that document.",
      { cause }
    );
  }

  return parseJsonOrThrow(response, "Atlas couldn't load that document.");
}

export { AtlasApiError };
