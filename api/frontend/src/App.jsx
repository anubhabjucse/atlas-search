import { useEffect, useState } from "react";
import SearchBar from "./components/SearchBar.jsx";
import SearchResults from "./components/SearchResults.jsx";
import SearchStats from "./components/SearchStats.jsx";
import LoadingResults from "./components/LoadingResults.jsx";
import EmptyState from "./components/EmptyState.jsx";
import ErrorState from "./components/ErrorState.jsx";
import DocumentViewer from "./components/DocumentViewer.jsx";
import { runSearch, fetchHealth, AtlasApiError } from "./api/atlasApi.js";

const DEFAULT_LIMIT = 10;

// idle -> searching -> success | error
export default function App() {
  const [query, setQuery] = useState("");
  const [submittedQuery, setSubmittedQuery] = useState("");
  const [status, setStatus] = useState("idle");
  const [results, setResults] = useState([]);
  const [stats, setStats] = useState(null);
  const [errorMessage, setErrorMessage] = useState("");
  const [openResult, setOpenResult] = useState(null);
  const [backendDown, setBackendDown] = useState(false);

  // Optional, non-blocking status check. Never gates the search UI.
  useEffect(() => {
    let cancelled = false;
    fetchHealth()
      .then((health) => {
        if (!cancelled) setBackendDown(health?.status !== "ok");
      })
      .catch(() => {
        if (!cancelled) setBackendDown(true);
      });
    return () => {
      cancelled = true;
    };
  }, []);

  async function performSearch(searchTerm) {
    setStatus("searching");
    setSubmittedQuery(searchTerm);
    setErrorMessage("");

    try {
      const data = await runSearch(searchTerm, DEFAULT_LIMIT);
      setResults(data.results || []);
      setStats(data.stats || null);
      setStatus("success");
    } catch (error) {
      const message =
        error instanceof AtlasApiError
          ? error.message
          : "Atlas couldn't complete that search.";
      console.error("Atlas search failed:", error);
      setErrorMessage(message);
      setStatus("error");
    }
  }

  function handleRetry() {
    if (submittedQuery) performSearch(submittedQuery);
  }

  function handleReset() {
    setQuery("");
    setSubmittedQuery("");
    setStatus("idle");
    setResults([]);
    setStats(null);
    setErrorMessage("");
  }

  const hasSearched = status !== "idle";

  return (
    <div className="app">
      <header className={`app-header ${hasSearched ? "app-header--results" : ""}`}>
        <div className="brand-row">
          <button type="button" className="brand" onClick={handleReset}>
            Atlas
          </button>
          {backendDown && (
            <span className="status-note status-note--down" role="status">
              Search service unreachable
            </span>
          )}
        </div>
      </header>

      <main className="app-main">
        {!hasSearched ? (
          <div className="hero">
            <h1 className="hero-title">Atlas</h1>
            <p className="hero-subtitle">Search the news archive.</p>
            <SearchBar
              query={query}
              onQueryChange={setQuery}
              onSubmit={performSearch}
              isSearching={false}
              showHints
            />
          </div>
        ) : (
          <div className="results-page">
            <div className="results-searchbar">
              <SearchBar
                query={query}
                onQueryChange={setQuery}
                onSubmit={performSearch}
                isSearching={status === "searching"}
                showHints={false}
              />
            </div>

            {status !== "searching" && (
              <p className="query-summary">
                <span>
                  Results for <strong>"{submittedQuery}"</strong>
                </span>
                {status === "success" && <span>{results.length} found</span>}
              </p>
            )}

            {status === "searching" && <LoadingResults />}

            {status === "error" && (
              <ErrorState message={errorMessage} onRetry={handleRetry} />
            )}

            {status === "success" && results.length === 0 && (
              <EmptyState query={submittedQuery} />
            )}

            {status === "success" && results.length > 0 && (
              <>
                <SearchResults results={results} onOpenResult={setOpenResult} />
                <SearchStats stats={stats} />
              </>
            )}
          </div>
        )}
      </main>

      <footer className="app-footer">Atlas V4 · lexical search over the news archive</footer>

      {openResult && (
        <DocumentViewer result={openResult} onClose={() => setOpenResult(null)} />
      )}
    </div>
  );
}
