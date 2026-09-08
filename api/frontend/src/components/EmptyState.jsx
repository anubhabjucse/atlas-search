export default function EmptyState({ query }) {
  return (
    <div className="state-block" role="status">
      <h2 className="state-title">No results for "{query}"</h2>
      <p className="state-body">Nothing in the archive matched that search. A few things that usually help:</p>
      <ul className="state-list">
        <li>Use fewer words</li>
        <li>Check your spelling</li>
        <li>Try a broader or more general term</li>
      </ul>
    </div>
  );
}
