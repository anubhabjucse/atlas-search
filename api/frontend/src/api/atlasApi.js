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


/**
 * POST /api/ai-survey
 * Vercel serverless function: article fetching + Gemini stay off the client.
 */
export async function fetchAiSurvey(query, results) {
  let response;
  try {
    response = await fetch("/api/ai-survey", {
      method: "POST",
      headers: JSON_HEADERS,
      body: JSON.stringify({
        query,
        documents: (results || []).slice(0, 5).map((result) => {
          const document = result?.document || {};
          return {
            id: result?.documentId ?? document?._id,
            headline: document?.headline,
            description: document?.short_description,
            url: document?.link,
          };
        }),
      }),
    });
  } catch (cause) {
    throw new AtlasApiError(
      "Couldn't reach the AI survey service. Check that the backend is running.",
      { cause }
    );
  }

  let body;
  try {
    body = JSON.parse(await response.text());
  } catch (cause) {
    body = null;
  }
  if (!response.ok) {
    const message = typeof body?.error === "string"
      ? body.error
      : response.status === 404
        ? "The backend running on port 3000 does not have the AI survey route yet. Restart or rebuild the backend to load the updated endpoint."
        : `AI summary unavailable (HTTP ${response.status}).`;
    throw new AtlasApiError(message, { status: response.status });
  }
  if (typeof body?.summary !== "string" || !body.summary.trim()) {
    throw new AtlasApiError("The AI survey service returned an invalid response.", {
      status: response.status,
    });
  }
  return body;
}

export { AtlasApiError };
