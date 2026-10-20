import { useEffect, useState } from "react";

function renderInline(text, keyPrefix) {
  const parts = text.split(/(\[[0-9]+\])/g);
  return parts.map((part, index) => {
    const match = part.match(/^\[([0-9]+)\]$/);
    return match
      ? <span className="ai-survey-citation" key={`${keyPrefix}-${index}`}>[{match[1]}]</span>
      : <span key={`${keyPrefix}-${index}`}>{part}</span>;
  });
}

function renderMarkdownLite(markdown) {
  const lines = markdown.split(/\n/);
  const blocks = [];
  let listItems = [];
  const flushList = () => {
    if (listItems.length) {
      blocks.push(<ul key={`ul-${blocks.length}`}>{listItems}</ul>);
      listItems = [];
    }
  };
  lines.forEach((line, index) => {
    const trimmed = line.trim();
    if (!trimmed) { flushList(); return; }
    if (/^[-*]\s+/.test(trimmed)) {
      listItems.push(<li key={`li-${index}`}>{renderInline(trimmed.replace(/^[-*]\s+/, ""), `li-${index}`)}</li>);
      return;
    }
    flushList();
    if (/^#{1,3}\s+/.test(trimmed)) {
      blocks.push(<h4 key={index}>{renderInline(trimmed.replace(/^#{1,3}\s+/, ""), `h-${index}`)}</h4>);
    } else {
      blocks.push(<p key={index}>{renderInline(trimmed, `p-${index}`)}</p>);
    }
  });
  flushList();
  return blocks;
}

export default function AiSurvey({ status, summary, sources, errorMessage, onRetry }) {
  const [expanded, setExpanded] = useState(true);
  useEffect(() => { if (status === "loading" || status === "ready") setExpanded(true); }, [status]);
  if (status === "idle") return null;

  return (
    <section className="ai-survey" aria-live="polite" aria-label="Atlas AI Survey">
      <div className="ai-survey-header">
        <div>
          <div className="ai-survey-eyebrow">ATLAS AI</div>
          <h2>AI Survey</h2>
        </div>
        {status === "ready" && (
          <button className="ai-survey-toggle" type="button" onClick={() => setExpanded(value => !value)}>
            {expanded ? "Collapse" : "Read survey"}
          </button>
        )}
      </div>

      {status === "loading" && (
        <div className="ai-survey-state"><span className="ai-survey-spinner" aria-hidden="true" />Loading AI summary…</div>
      )}

      {status === "unavailable" && (
        <div className="ai-survey-state">
          <p>{errorMessage || "AI summary unavailable"}</p>
          <button className="ai-survey-retry" type="button" onClick={onRetry}>Try again</button>
        </div>
      )}

      {status === "ready" && expanded && (
        <>
          <div className="ai-survey-body">{renderMarkdownLite(summary || "")}</div>
          {sources?.length > 0 && (
            <div className="ai-survey-source-list">
              <h3>Retrieved sources</h3>
              <ol>
                {sources.map((source, index) => (
                  <li key={`${source.id}-${source.url}`}>
                    <a href={source.url} target="_blank" rel="noopener noreferrer">[{index + 1}] {source.headline}</a>
                  </li>
                ))}
              </ol>
            </div>
          )}
        </>
      )}
    </section>
  );
}
