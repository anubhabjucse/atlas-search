export default function LoadingResults({ count = 5 }) {
  return (
    <div aria-live="polite" aria-busy="true">
      <p className="searching-note">Searching Atlas…</p>
      <ul className="skeleton-list" aria-hidden="true">
        {Array.from({ length: count }).map((_, index) => (
          <li className="skeleton-card" key={index}>
            <div className="skeleton-line skeleton-line--title" />
            <div className="skeleton-line skeleton-line--body" />
            <div className="skeleton-line skeleton-line--meta" />
          </li>
        ))}
      </ul>
    </div>
  );
}
