import { useEffect, useRef, useState } from "react";
import { fetchDocument, AtlasApiError } from "../api/atlasApi.js";

function formatDate(dateString) {
  if (!dateString) return null;
  const parsed = new Date(dateString);
  if (Number.isNaN(parsed.getTime())) return dateString;
  return parsed.toLocaleDateString(undefined, {
    year: "numeric",
    month: "long",
    day: "numeric",
  });
}

/**
 * Shows the full document for a search result in a modal dialog.
 * Renders instantly with the summary already in hand from the search
 * response, then fills in anything extra from GET /api/documents/:id.
 */
export default function DocumentViewer({ result, onClose }) {
  const [doc, setDoc] = useState(result.document || null);
  const [status, setStatus] = useState("loading"); // loading | ready | error
  const [errorMessage, setErrorMessage] = useState("");
  const panelRef = useRef(null);
  const closeButtonRef = useRef(null);

  useEffect(() => {
    let cancelled = false;

    async function load() {
      try {
        const data = await fetchDocument(result.documentId);
        if (cancelled) return;
        setDoc(data);
        setStatus("ready");
      } catch (error) {
        if (cancelled) return;
        const message =
          error instanceof AtlasApiError
            ? "Atlas couldn't load the full document. Showing what's available below."
            : "Something went wrong loading this document.";
        setErrorMessage(message);
        // Keep showing the summary we already had, if any.
        setStatus(doc ? "ready" : "error");
      }
    }

    load();
    return () => {
      cancelled = true;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [result.documentId]);

  useEffect(() => {
    closeButtonRef.current?.focus();

    function handleKeyDown(event) {
      if (event.key === "Escape") {
        onClose();
      }
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [onClose]);

  function handleOverlayClick(event) {
    if (event.target === event.currentTarget) onClose();
  }

  const headline = doc?.headline;
  const description = doc?.short_description;
  const category = doc?.category;
  const authors = doc?.authors;
  const formattedDate = formatDate(doc?.date);
  const link = doc?.link;

  return (
    <div className="doc-overlay" onMouseDown={handleOverlayClick}>
      <div
        className="doc-panel"
        role="dialog"
        aria-modal="true"
        aria-labelledby="doc-viewer-title"
        ref={panelRef}
      >
        <div className="doc-panel-top">
          <button
            type="button"
            className="doc-close"
            aria-label="Close document"
            onClick={onClose}
            ref={closeButtonRef}
          >
            ×
          </button>
        </div>

        {status === "error" && !headline ? (
          <p className="doc-state" role="alert">
            {errorMessage || "Atlas couldn't load this document."}
          </p>
        ) : (
          <>
            <h2 id="doc-viewer-title" className="doc-headline">
              {headline || "Untitled document"}
            </h2>

            <div className="doc-meta">
              {category && <span>{category}</span>}
              {category && (formattedDate || authors) && <span className="doc-meta-sep">·</span>}
              {formattedDate && <span>{formattedDate}</span>}
              {formattedDate && authors && <span className="doc-meta-sep">·</span>}
              {authors && <span>{authors}</span>}
            </div>

            {description && <p className="doc-body">{description}</p>}

            {status === "loading" && <p className="doc-state">Loading full document…</p>}
            {status === "ready" && errorMessage && <p className="doc-state">{errorMessage}</p>}

            {link && (
              <a className="doc-link" href={link} target="_blank" rel="noopener noreferrer">
                Read the original article ↗
                <span className="visually-hidden">(opens in a new tab)</span>
              </a>
            )}
          </>
        )}
      </div>
    </div>
  );
}
