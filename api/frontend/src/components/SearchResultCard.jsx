function formatDate(dateString) {
  if (!dateString) return null;
  const parsed = new Date(dateString);
  if (Number.isNaN(parsed.getTime())) return dateString;
  return parsed.toLocaleDateString(undefined, {
    year: "numeric",
    month: "short",
    day: "numeric",
  });
}

export default function SearchResultCard({ result, onOpen }) {
  const { document, score } = result;
  const { headline, short_description, category, authors, date } = document || {};
  const formattedDate = formatDate(date);

  return (
    <li className="result-card">
      <h2 className="result-headline">
        <button type="button" className="result-headline-btn" onClick={() => onOpen(result)}>
          {headline || "Untitled document"}
        </button>
      </h2>

      {short_description && <p className="result-description">{short_description}</p>}

      <div className="result-meta">
        {category && <span>{category}</span>}
        {category && (formattedDate || authors) && <span className="result-meta-sep">·</span>}
        {formattedDate && <span>{formattedDate}</span>}
        {formattedDate && authors && <span className="result-meta-sep">·</span>}
        {authors && <span>{authors}</span>}
        {typeof score === "number" && (
          <>
            <span className="result-meta-sep">·</span>
            <span className="result-score">relevance {score.toFixed(2)}</span>
          </>
        )}
      </div>
    </li>
  );
}
