"""Validate compact Tron results using trusted challenge rules and frozen references.

This module never compiles or runs a submitted program. Both the local checker and
the write-enabled publisher use it; artifact scores are not trusted on their own.
"""
import importlib.util
import json
import math
from pathlib import Path
import re


def number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def validate_result(doc, root):
    root = Path(root)
    spec = importlib.util.spec_from_file_location("trusted_tron_challenge", root / "tron/challenge.py")
    challenge = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(challenge)
    table = json.loads((root / "tron/results/results.json").read_text(encoding="utf-8"))["boards"]["强对手榜"]["score"]["table"]
    if not isinstance(doc, dict):
        raise ValueError("光轮成绩不是对象")
    budget = doc.get("budget")
    if (type(doc.get("schema")) is not int or doc["schema"] != 1 or doc.get("complete") is not True or
            doc.get("mode") != "formal" or doc.get("rating_method") != "fixed-final-strong" or
            not isinstance(budget, dict) or budget.get("budget_kind") != "cpu" or
            type(budget.get("first_ms")) is not int or budget["first_ms"] != 2000 or
            type(budget.get("turn_ms")) is not int or budget["turn_ms"] != 50 or
            type(doc.get("full")) is not bool or type(doc.get("seed")) is not int or doc["seed"] != challenge.SEED or
            doc.get("weights") != challenge.MODES or
            doc.get("reference") != {"path": "tron/results/results.json", "board": "强对手榜", "anchor": "Doubao"} or
            not isinstance(doc.get("source_sha256"), str) or not re.fullmatch(r"[0-9a-f]{64}", doc["source_sha256"])):
        raise ValueError("光轮成绩缺少正式 CPU 预算、固定强对手榜口径或源码哈希")
    names = list(challenge.OPPONENTS)
    games = challenge.schedule(names, doc["full"])
    expected = {m: {n: 0 for n in names} for m in challenge.MODES}
    counts = {m: 0 for m in challenge.MODES}
    for mode, _, _, seats in games:
        counts[mode] += 1
        for name in seats:
            if name != "me":
                expected[mode][name] += 1
    if (type(doc.get("games")) is not int or doc["games"] != len(games) or doc.get("games_by_mode") != counts or
            any(type(v) is not int for v in (doc.get("games_by_mode") or {}).values())):
        raise ValueError("光轮成绩局数不完整")
    opponents = doc.get("opponents")
    if (not isinstance(opponents, list) or len(opponents) != len(names) or
            any(not isinstance(r, dict) or not isinstance(r.get("name"), str) for r in opponents) or
            {r["name"] for r in opponents} != set(names)):
        raise ValueError("光轮成绩对手不完整或重复")
    by_name = {r["name"]: r for r in opponents}
    for name, row in by_name.items():
        reference = table[name]
        refs = {m: reference["modes"][m]["score"] or 0.0 for m in challenge.MODES}
        if (row.get("ranked") is not (name != "sparring_hard") or not number(row.get("rating")) or
                row["rating"] != reference["total"] or not isinstance(row.get("ratings"), dict) or
                any(not number(v) for v in row["ratings"].values()) or row["ratings"] != refs or not isinstance(row.get("tally"), dict) or
                set(row["tally"]) != set(challenge.MODES)):
            raise ValueError("光轮固定参照评分或排名身份不一致")
        for mode in challenge.MODES:
            tally = row["tally"][mode]
            if (not isinstance(tally, list) or len(tally) != 3 or
                    any(type(v) is not int or v < 0 for v in tally) or sum(tally) != expected[mode][name]):
                raise ValueError("光轮逐对手胜平负和固定赛程不符")
    ratings = {m: challenge.fit([(by_name[n]["ratings"][m], *by_name[n]["tally"][m])
                                for n in names if expected[m][n]]) for m in challenge.MODES}
    total = sum(challenge.MODES[m] * ratings[m] for m in challenge.MODES)
    rank = 1 + sum(table[n]["total"] > total for n in names if n != "sparring_hard")
    claimed = doc.get("ratings")
    if (not isinstance(claimed, dict) or set(claimed) != set(ratings) or
            any(not number(claimed[m]) or not math.isclose(claimed[m], ratings[m], abs_tol=1e-7) for m in ratings) or
            not number(doc.get("rating")) or not math.isclose(doc["rating"], total, abs_tol=1e-7) or
            type(doc.get("rank")) is not int or doc["rank"] != rank):
        raise ValueError("光轮分项、加权分或参照名次与可信重算不符")
    field = [{"name": n, "is_entry": False, "score": round(table[n]["total"], 1)}
             for n in names if n != "sparring_hard"]
    field.append({"name": "你的 bot", "is_entry": True, "score": round(total, 1)})
    keys = ("schema", "complete", "rating_method", "mode", "budget", "reference", "source_sha256", "seed", "full",
            "games", "games_by_mode", "weights", "ratings", "rating", "rank", "opponents")
    extra = {k: doc[k] for k in keys}
    extra["games_note"] = "%d 局（1v1 %d、4 人 %d、12 人 %d）" % (len(games), counts["1v1"], counts["3-4"], counts["12"])
    return field, extra
