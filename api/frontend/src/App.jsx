import { useEffect, useRef, useState } from "react";
import SearchBar from "./components/SearchBar.jsx";
import SearchResults from "./components/SearchResults.jsx";
import SearchStats from "./components/SearchStats.jsx";
import LoadingResults from "./components/LoadingResults.jsx";
import EmptyState from "./components/EmptyState.jsx";
import ErrorState from "./components/ErrorState.jsx";
import DocumentViewer from "./components/DocumentViewer.jsx";
import AiSurvey from "./components/AiSurvey.jsx";
import { runSearch, fetchHealth, fetchAiSurvey, AtlasApiError } from "./api/atlasApi.js";

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
  const [surveyStatus, setSurveyStatus] = useState("idle");
  const [surveySummary, setSurveySummary] = useState("");
  const [surveySources, setSurveySources] = useState([]);
  const [surveyError, setSurveyError] = useState("");
  const surveyRequestId = useRef(0);

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
    const requestId = ++surveyRequestId.current;
    setStatus("searching");
    setSubmittedQuery(searchTerm);
    setErrorMessage("");
    setSurveyStatus("idle");
    setSurveySummary("");
    setSurveySources([]);
    setSurveyError("");

    try {
      const data = await runSearch(searchTerm, DEFAULT_LIMIT);
      if (requestId !== surveyRequestId.current) return;
      const searchResults = data.results || [];
      setResults(searchResults);
      setStats(data.stats || null);
      setStatus("success");

      if (searchResults.length > 0) {
        setSurveyStatus("loading");
        fetchAiSurvey(searchTerm, searchResults)
          .then((survey) => {
            if (requestId !== surveyRequestId.current) return;
            setSurveySummary(survey.summary);
            setSurveySources(survey.sources || []);
            setSurveyStatus("ready");
          })
          .catch((error) => {
            if (requestId !== surveyRequestId.current) return;
            console.warn("Atlas AI Survey unavailable:", error);
            setSurveyError(error.message || "AI summary unavailable.");
            setSurveyStatus("unavailable");
          });
      }
    } catch (error) {
      if (requestId !== surveyRequestId.current) return;
      const message =
        error instanceof AtlasApiError
          ? error.message
          : "Atlas couldn't complete that search.";
      console.error("Atlas search failed:", error);
      setErrorMessage(message);
      setStatus("error");
      setSurveyStatus("idle");
    }
  }

  async function retryAiSurvey() {
    if (!submittedQuery || !results.length) return;
    const requestId = ++surveyRequestId.current;
    setSurveyStatus("loading");
    setSurveyError("");
    try {
      const survey = await fetchAiSurvey(submittedQuery, results);
      if (requestId !== surveyRequestId.current) return;
      setSurveySummary(survey.summary);
      setSurveySources(survey.sources || []);
      setSurveyStatus("ready");
    } catch (error) {
      if (requestId !== surveyRequestId.current) return;
      console.warn("Atlas AI Survey unavailable:", error);
      setSurveyError(error.message || "AI summary unavailable.");
      setSurveyStatus("unavailable");
    }
  }

  function handleRetry() {
    if (submittedQuery) performSearch(submittedQuery);
  }

  function handleReset() {
    surveyRequestId.current += 1;
    setSurveyStatus("idle");
    setSurveySummary("");
    setSurveySources([]);
    setSurveyError("");
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
                <AiSurvey status={surveyStatus} summary={surveySummary} sources={surveySources} errorMessage={surveyError} onRetry={retryAiSurvey} />
                <SearchResults results={results} onOpenResult={setOpenResult} />
                <SearchStats stats={stats} />
              </>
            )}
          </div>
        )}
      </main>

      <footer className="app-footer">Atlas · search the news archive</footer>

      {openResult && (
        <DocumentViewer result={openResult} onClose={() => setOpenResult(null)} />
      )}
    </div>
  );
}
