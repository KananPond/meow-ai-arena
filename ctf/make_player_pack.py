"""按比赛时的条件，打一份发给新选手的 workspace.zip（第 3 集夺旗）。

  python make_player_pack.py                打到 out/workspace.zip
  python make_player_pack.py --out 目录      打到 <目录>/workspace.zip

仓库里的 workspace/ 放着基准的源码 baseline.cpp，比赛时选手只拿到编译好的 baseline.exe。
想让一个新模型按比赛时的条件参赛，不要把 workspace/ 文件夹直接给它，用这个脚本打包：

  1. 用比赛时的编译参数（-std=c++20 -O2，Windows 上加 -static）把 workspace/baseline.cpp 编译成 baseline.exe；
  2. 逐个核对 baseline.exe 以外的文件和 workspace_manifest.json 里记的 sha256，不一致就停下。
     arena.py、core.py 两个文件开源时改过（清单里有说明），核对的是开源版的哈希；其余和比赛时发出去的逐字节相同；
  3. 按比赛时压缩包的文件清单和顺序打包：顶层只有 workspace/，模板在 submission/bot.cpp，不放 baseline.cpp。

需要 Python 3.10+ 和 g++（Windows 可用 w64devkit，见仓库根目录 README）。
"""
import hashlib
import json
import os
import sys
import tempfile
import zipfile
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8")
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
WS = HERE / "workspace"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    args = sys.argv[1:]
    out_dir = Path(args[args.index("--out") + 1]).resolve() if "--out" in args else HERE / "out"
    manifest = json.loads((HERE / "workspace_manifest.json").read_text(encoding="utf-8"))

    # 1. 核对：除 baseline.exe 外，每个文件都要和清单对得上
    contents, bad = {}, []
    for item in manifest["files"]:
        if item.get("dir"):
            continue
        rel = item["path"].removeprefix("workspace/")
        if rel == "baseline.exe":
            continue
        path = WS / rel
        if not path.is_file():
            bad.append(f"缺少 {path}")
            continue
        data = path.read_bytes()
        if sha(data) != item.get("open_source_sha256", item["sha256"]):
            bad.append(f"和清单不一致：{path}")
        contents[item["path"]] = data
    if bad:
        raise SystemExit("workspace/ 里的文件和清单对不上，没有打包：\n  " + "\n  ".join(bad))

    # 2. 编译基准：用选手包自己的编译函数（找编译器的办法、编译参数都和本地对战时一样）
    sys.path.insert(0, str(WS))
    import core  # noqa: E402
    with tempfile.TemporaryDirectory(prefix="ctf-pack-") as tmp:
        try:
            exe = core.compile_cpp(WS / "baseline.cpp", Path(tmp))
        except RuntimeError as exc:
            raise SystemExit(f"基准编译失败：{exc}（Windows 推荐 w64devkit，放法见仓库根目录 README）")
        exe_data = Path(exe).read_bytes()
    contents["workspace/baseline.exe"] = exe_data

    # 3. 按比赛时的清单和顺序打包
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / "workspace.zip"
    tmp_zip = out.with_name(out.name + ".tmp")
    with zipfile.ZipFile(tmp_zip, "w", zipfile.ZIP_DEFLATED) as z:
        for item in manifest["files"]:
            if item.get("dir"):
                z.writestr(zipfile.ZipInfo(item["path"]), b"")
            else:
                z.writestr(item["path"], contents[item["path"]])
    os.replace(tmp_zip, out)
    with zipfile.ZipFile(out) as z:
        names = [n for n in z.namelist() if not n.endswith("/")]
    assert not any(n.endswith("baseline.cpp") for n in names)

    original = next(i["sha256"] for i in manifest["files"] if i["path"].endswith("baseline.exe"))
    print(f"已生成：{out}")
    print(f"共 {len(names)} 个文件（和比赛时的清单相同）；baseline.exe 由源码现编，sha256 {sha(exe_data)[:12]}…"
          f"（比赛时那份是 {original[:12]}…，可执行文件的哈希每次编译都可能不同）")
    print("把它和 prompt.md 里的提示词交给模型；不要把 workspace/ 文件夹直接给模型，里面有基准源码。")


if __name__ == "__main__":
    main()
