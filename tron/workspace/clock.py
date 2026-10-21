"""查看比赛用时。只读，不写任何文件。从解压出本文件的那一刻算起：推荐 45 分钟，60 分钟强制收卷。"""
import sys
import time
from pathlib import Path

TARGET, LIMIT = 45, 60  # 分钟：推荐完成时间、强制收卷


def born(path):
    """文件在本机被创建（解压出来）的时刻。不能用修改时间：解压会把它恢复成打包时的值。"""
    st = Path(path).stat()
    return getattr(st, "st_birthtime", None) or st.st_ctime


def message(used):
    if used < TARGET:
        return f"已用 {used:.1f} 分钟。推荐在 {TARGET} 分钟内完成（还有 {TARGET - used:.1f} 分钟），{LIMIT} 分钟强制收卷。"
    if used < LIMIT:
        return f"已用 {used:.1f} 分钟，已过推荐的 {TARGET} 分钟，请尽快收尾。{LIMIT} 分钟强制收卷，还剩 {LIMIT - used:.1f} 分钟。"
    return f"已用 {used:.1f} 分钟，已到 {LIMIT} 分钟，强制收卷：请立即停止修改并提交。"


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass
    print(message(max(0.0, time.time() - born(__file__)) / 60))
