import SearchResultCard from "./SearchResultCard.jsx";

export default function SearchResults({ results, onOpenResult }) {
  return (
    <ul className="results-list">
      {results.map((result) => (
        <SearchResultCard
          key={result.documentId}
          result={result}
          onOpen={onOpenResult}
        />
      ))}
    </ul>
  );
}
