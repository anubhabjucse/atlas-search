import { useRef } from "react";
import QueryInput from "./QueryInput.jsx";
import SearchButton from "./SearchButton.jsx";

const EXAMPLE_QUERIES = ["climate change", "technology startups", '"Donald Trump"', "climate AND policy"];

/**
 * SearchBar is the single place the search UI lives. It is deliberately
 * split into QueryInput and SearchButton so a future V5 SuggestionList
 * (autocomplete / semantic suggestions) can be slotted in here later
 * without changing how App.jsx uses this component.
 */
export default function SearchBar({ query, onQueryChange, onSubmit, isSearching, showHints }) {
  const inputRef = useRef(null);

  function handleSubmit(event) {
    event?.preventDefault?.();
    const trimmed = query.trim();
    if (!trimmed || isSearching) return;
    onSubmit(trimmed);
  }

  function handleHintClick(hint) {
    onQueryChange(hint);
    inputRef.current?.focus();
  }

  return (
    <div>
      <form className="search-form" role="search" onSubmit={handleSubmit}>
        <QueryInput
          value={query}
          onChange={onQueryChange}
          onSubmit={handleSubmit}
          inputRef={inputRef}
        />
        <SearchButton disabled={!query.trim() || isSearching} isSearching={isSearching} />
      </form>

      {/* Future: <SuggestionList /> renders here, driven by query state above. */}

      {showHints && (
        <p className="search-hints">
          <span className="search-hints-label">Try:</span>
          {EXAMPLE_QUERIES.map((hint, index) => (
            <span key={hint}>
              {index > 0 && <span className="search-hint-sep">·</span>}
              <button type="button" className="search-hint-chip" onClick={() => handleHintClick(hint)}>
                {hint}
              </button>
            </span>
          ))}
        </p>
      )}
    </div>
  );
}
