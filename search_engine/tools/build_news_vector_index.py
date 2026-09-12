#!/usr/bin/env python3

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


DEFAULT_EMBEDDING_URL = "http://localhost:8080/embedding"
DEFAULT_BATCH_SIZE = 32
EXPECTED_DIMENSIONS = 768


def parse_args():
    parser = argparse.ArgumentParser(
        description="Build an Atlas vector index using EmbeddingGemma."
    )

    parser.add_argument(
        "dataset",
        help="Path to the News Category JSONL dataset",
    )

    parser.add_argument(
        "output",
        help="Path to the output vector index",
    )

    parser.add_argument(
        "--embedding-url",
        default=DEFAULT_EMBEDDING_URL,
        help=(
            "llama.cpp embedding endpoint "
            f"(default: {DEFAULT_EMBEDDING_URL})"
        ),
    )

    parser.add_argument(
        "--batch-size",
        type=int,
        default=DEFAULT_BATCH_SIZE,
        help=f"Documents per embedding request (default: {DEFAULT_BATCH_SIZE})",
    )

    parser.add_argument(
        "--max-documents",
        type=int,
        default=None,
        help="Optional limit for testing a small subset",
    )

    parser.add_argument(
        "--timeout",
        type=float,
        default=120.0,
        help="HTTP request timeout in seconds",
    )

    return parser.parse_args()


def extract_text(record):
    headline = record.get("headline")

    if not isinstance(headline, str) or not headline:
        return None

    description = record.get("short_description", "")

    if not isinstance(description, str):
        description = ""

    return headline + "\n" + description


def request_embeddings(url, texts, timeout):
    payload = json.dumps({
        "input": texts,
    }).encode("utf-8")

    request = urllib.request.Request(
        url,
        data=payload,
        headers={
            "Content-Type": "application/json",
        },
        method="POST",
    )

    try:
        with urllib.request.urlopen(
            request,
            timeout=timeout,
        ) as response:
            body = response.read()

    except urllib.error.HTTPError as error:
        details = error.read().decode(
            "utf-8",
            errors="replace",
        )

        raise RuntimeError(
            f"Embedding server returned HTTP {error.code}: {details}"
        ) from error

    except urllib.error.URLError as error:
        raise RuntimeError(
            f"Unable to reach embedding server at {url}: {error}"
        ) from error

    try:
        result = json.loads(body)
    except json.JSONDecodeError as error:
        raise RuntimeError(
            "Embedding server returned invalid JSON"
        ) from error

    if not isinstance(result, list):
        raise RuntimeError(
            "Embedding response must be a list"
        )

    if len(result) != len(texts):
        raise RuntimeError(
            "Embedding count mismatch: "
            f"requested {len(texts)}, received {len(result)}"
        )

    # llama.cpp returns:
    #
    # [
    #   {
    #     "index": 0,
    #     "embedding": [[...]]
    #   },
    #   ...
    # ]
    #
    # Sort by the returned index so the embedding order always
    # matches the input document order.

    ordered = [None] * len(texts)

    for item in result:
        if not isinstance(item, dict):
            raise RuntimeError(
                "Embedding response contains a non-object result"
            )

        index = item.get("index")
        embedding = item.get("embedding")

        if not isinstance(index, int):
            raise RuntimeError(
                "Embedding response contains an invalid index"
            )

        if index < 0 or index >= len(texts):
            raise RuntimeError(
                f"Embedding response contains out-of-range index: {index}"
            )

        if not isinstance(embedding, list):
            raise RuntimeError(
                f"Embedding {index} is not a list"
            )

        # llama.cpp currently wraps the vector in another list.
        if (
            len(embedding) == 1
            and isinstance(embedding[0], list)
        ):
            embedding = embedding[0]

        if not embedding:
            raise RuntimeError(
                f"Embedding {index} is empty"
            )

        ordered[index] = embedding

    if any(embedding is None for embedding in ordered):
        raise RuntimeError(
            "Embedding response is missing one or more indices"
        )

    return ordered


def validate_embedding(embedding, document_id):
    if not isinstance(embedding, list):
        raise RuntimeError(
            f"Document {document_id}: embedding is not a list"
        )

    if len(embedding) != EXPECTED_DIMENSIONS:
        raise RuntimeError(
            f"Document {document_id}: expected "
            f"{EXPECTED_DIMENSIONS} dimensions, "
            f"received {len(embedding)}"
        )

    for value in embedding:
        if not isinstance(value, (int, float)):
            raise RuntimeError(
                f"Document {document_id}: embedding contains "
                "a non-numeric value"
            )


def load_vector_index_module():
    """
    The builder uses a small C++ helper executable to write the
    VectorIndex binary format.

    This function intentionally fails with a clear message until
    that helper is available.
    """
    return None


def write_vectors(
    vectors_path,
    document_ids,
    embeddings,
):
    """
    Temporary portable writer for the current ATLVEC01 format.

    The binary layout mirrors VectorIndex::save():

        magic[8]
        uint32 version
        uint32 dimensions
        uint64 document_count
        document IDs
        float32 embeddings
    """

    import struct

    if len(document_ids) != len(embeddings):
        raise RuntimeError(
            "Document/vector count mismatch"
        )

    dimensions = (
        len(embeddings[0])
        if embeddings
        else 0
    )

    output = Path(vectors_path)
    output.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    with output.open("wb") as file:
        file.write(
            struct.pack(
                "<8sIIQ",
                b"ATLVEC01",
                1,
                dimensions,
                len(document_ids),
            )
        )

        for document_id in document_ids:
            file.write(
                struct.pack(
                    "<I",
                    document_id,
                )
            )

        for embedding in embeddings:
            file.write(
                struct.pack(
                    f"<{dimensions}f",
                    *embedding,
                )
            )


def main():
    args = parse_args()

    if args.batch_size <= 0:
        raise RuntimeError(
            "--batch-size must be greater than zero"
        )

    dataset_path = Path(args.dataset)
    output_path = Path(args.output)

    if not dataset_path.exists():
        raise RuntimeError(
            f"Dataset does not exist: {dataset_path}"
        )

    print("Atlas News Vector Index Builder")
    print("================================")
    print(f"Dataset        : {dataset_path}")
    print(f"Output         : {output_path}")
    print(f"Embedding URL  : {args.embedding_url}")
    print(f"Batch size     : {args.batch_size}")

    if args.max_documents is not None:
        print(f"Max documents  : {args.max_documents}")

    print()

    document_ids = []
    embeddings = []

    line_number = 0
    documents_seen = 0
    documents_indexed = 0
    invalid_records = 0
    missing_headlines = 0

    start_time = time.time()

    pending_ids = []
    pending_texts = []

    def process_batch():
        nonlocal pending_ids
        nonlocal pending_texts

        if not pending_texts:
            return

        batch_embeddings = request_embeddings(
            args.embedding_url,
            pending_texts,
            args.timeout,
        )

        for document_id, embedding in zip(
            pending_ids,
            batch_embeddings,
        ):
            validate_embedding(
                embedding,
                document_id,
            )

            document_ids.append(document_id)
            embeddings.append(embedding)

        pending_ids = []
        pending_texts = []

    with dataset_path.open(
        "r",
        encoding="utf-8",
    ) as input_file:

        for line in input_file:
            line_number += 1

            if not line.strip():
                continue

            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                invalid_records += 1
                continue

            documents_seen += 1

            content = extract_text(record)

            if content is None:
                missing_headlines += 1
                continue

            document_id = documents_indexed + 1
            documents_indexed += 1

            pending_ids.append(document_id)
            pending_texts.append(content)

            if len(pending_texts) >= args.batch_size:
                process_batch()

                elapsed = time.time() - start_time

                print(
                    f"  Embedded {documents_indexed} documents "
                    f"({elapsed:.1f}s)"
                )
                sys.stdout.flush()

            if (
                args.max_documents is not None
                and documents_indexed >= args.max_documents
            ):
                break

    process_batch()

    if not embeddings:
        raise RuntimeError(
            "No embeddings were generated"
        )

    dimensions = len(embeddings[0])

    if dimensions != EXPECTED_DIMENSIONS:
        raise RuntimeError(
            f"Expected {EXPECTED_DIMENSIONS}-dimensional embeddings, "
            f"received {dimensions}"
        )

    write_vectors(
        output_path,
        document_ids,
        embeddings,
    )

    elapsed = time.time() - start_time

    print()
    print("Vector index build complete.")
    print("----------------------------")
    print(f"Input lines       : {line_number}")
    print(f"Documents seen    : {documents_seen}")
    print(f"Documents indexed : {documents_indexed}")
    print(f"Missing headlines : {missing_headlines}")
    print(f"Invalid records   : {invalid_records}")
    print(f"Dimensions        : {dimensions}")
    print(f"Output            : {output_path}")
    print(f"Elapsed           : {elapsed:.1f}s")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(
            "\nVector index build interrupted.",
            file=sys.stderr,
        )
        sys.exit(130)
    except Exception as error:
        print(
            f"\nVector index build failed: {error}",
            file=sys.stderr,
        )
        sys.exit(1)