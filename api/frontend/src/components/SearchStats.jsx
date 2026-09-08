const STAT_LABELS = {
  documentsScored: "Documents scored",
  postingsVisited: "Postings visited",
  blocksSkipped: "Blocks skipped",
  queryTerms: "Query terms",
  matchedTerms: "Matched terms",
  candidatesConsidered: "Candidates considered",
};

export default function SearchStats({ stats }) {
  if (!stats) return null;

  const entries = Object.entries(STAT_LABELS).filter(([key]) => key in stats);
  if (entries.length === 0) return null;

  return (
    <details className="stats-disclosure">
      <summary className="stats-summary">About this search</summary>
      <dl className="stats-grid">
        {entries.map(([key, label]) => (
          <div key={key}>
            <dt>{label}</dt>
            <dd>{stats[key]}</dd>
          </div>
        ))}
      </dl>
    </details>
  );
}
