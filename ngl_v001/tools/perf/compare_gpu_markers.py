#!/usr/bin/env python3
"""compare_gpu_markers.py: Compare benchmark marker JSON and write Markdown report."""

import argparse
import json
from pathlib import Path


def load_markers(path: Path):
    data = json.loads(path.read_text(encoding="utf-8"))
    markers = {}
    for row in data.get("markers", []):
        scope = row.get("scope")
        if not scope:
            continue
        markers[scope] = {
            "sample_count": int(row.get("sample_count", 0)),
            "last_ms": float(row.get("last_ms", 0.0)),
            "avg_ms": float(row.get("avg_ms", 0.0)),
            "p50_ms": float(row.get("p50_ms", 0.0)),
            "p95_ms": float(row.get("p95_ms", 0.0)),
            "max_ms": float(row.get("max_ms", 0.0)),
            "frame_id": int(row.get("frame_id", 0)),
        }
    return data, markers


def fmt_ms(v: float) -> str:
    return f"{v:.4f}"


def fmt_pct(v: float) -> str:
    sign = "+" if v >= 0 else ""
    return f"{sign}{v:.2f}%"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--baseline", default="")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    candidate_path = Path(args.candidate)
    baseline_path = Path(args.baseline) if args.baseline else None
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    c_meta, c_markers = load_markers(candidate_path)

    lines = []
    lines.append("# GPU Benchmark Report")
    lines.append("")
    lines.append(f"- Candidate: `{candidate_path}`")
    lines.append(f"- Candidate tag: `{c_meta.get('tag', '')}`")
    lines.append(f"- Candidate build: `{c_meta.get('build_config', '')}`")
    lines.append(f"- Candidate captured frames: `{c_meta.get('measure_frames_captured', 0)}`")
    lines.append("")

    if baseline_path:
        b_meta, b_markers = load_markers(baseline_path)
        lines.append(f"- Baseline: `{baseline_path}`")
        lines.append(f"- Baseline tag: `{b_meta.get('tag', '')}`")
        lines.append("")

        union_scopes = sorted(set(c_markers.keys()) | set(b_markers.keys()))
        rows = []
        for scope in union_scopes:
            b = b_markers.get(scope)
            c = c_markers.get(scope)
            b_avg = b["avg_ms"] if b else 0.0
            c_avg = c["avg_ms"] if c else 0.0
            diff_ms = c_avg - b_avg
            diff_pct = (diff_ms / b_avg * 100.0) if b_avg > 0.0 else 0.0
            rows.append((scope, b_avg, c_avg, diff_ms, diff_pct))

        rows.sort(key=lambda r: r[4], reverse=True)
        lines.append("## Avg ms delta (candidate - baseline)")
        lines.append("")
        lines.append("| scope | baseline_avg_ms | candidate_avg_ms | delta_ms | delta_pct |")
        lines.append("| --- | ---: | ---: | ---: | ---: |")
        for scope, b_avg, c_avg, diff_ms, diff_pct in rows:
            lines.append(
                f"| `{scope}` | {fmt_ms(b_avg)} | {fmt_ms(c_avg)} | {fmt_ms(diff_ms)} | {fmt_pct(diff_pct)} |"
            )
    else:
        rows = sorted(c_markers.items(), key=lambda kv: kv[1]["avg_ms"], reverse=True)
        lines.append("## Candidate marker summary (avg ms desc)")
        lines.append("")
        lines.append("| scope | sample_count | last_ms | avg_ms | p50_ms | p95_ms | max_ms |")
        lines.append("| --- | ---: | ---: | ---: | ---: | ---: | ---: |")
        for scope, s in rows:
            lines.append(
                f"| `{scope}` | {s['sample_count']} | {fmt_ms(s['last_ms'])} | {fmt_ms(s['avg_ms'])} | "
                f"{fmt_ms(s['p50_ms'])} | {fmt_ms(s['p95_ms'])} | {fmt_ms(s['max_ms'])} |"
            )

    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
