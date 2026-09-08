export default function ErrorState({ message, onRetry }) {
  return (
    <div className="state-block state-block--error" role="alert">
      <h2 className="state-title">Atlas couldn't complete that search</h2>
      <p className="state-body">
        {message || "Please check that the search service is running and try again."}
      </p>
      {onRetry && (
        <button type="button" className="retry-button" onClick={onRetry}>
          Try again
        </button>
      )}
    </div>
  );
}
