export default function SearchButton({ disabled, isSearching }) {
  return (
    <button type="submit" className="search-submit" disabled={disabled}>
      {isSearching ? "Searching…" : "Search"}
    </button>
  );
}
