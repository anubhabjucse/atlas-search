export default function QueryInput({ value, onChange, onSubmit, inputRef }) {
  function handleKeyDown(event) {
    if (event.key === "Enter") {
      event.preventDefault();
      onSubmit();
    }
  }

  return (
    <div className="search-input-wrap">
      <label htmlFor="atlas-query" className="visually-hidden">
        Search Atlas
      </label>
      <input
        id="atlas-query"
        ref={inputRef}
        className="search-input"
        type="text"
        role="searchbox"
        autoComplete="off"
        autoCorrect="off"
        spellCheck="false"
        placeholder="Search the archive…"
        value={value}
        onChange={(event) => onChange(event.target.value)}
        onKeyDown={handleKeyDown}
      />
      {value.length > 0 && (
        <button
          type="button"
          className="search-clear"
          aria-label="Clear search"
          onClick={() => onChange("")}
        >
          ×
        </button>
      )}
    </div>
  );
}
