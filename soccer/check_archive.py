"""检查相对链接、归档哈希、赛程、上场源码与逐局积分的一致性。"""
import gzip
import hashlib
import json
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'tournament'))
from score import standings


def read(path):
    raw = path.read_bytes()
    return json.loads(gzip.decompress(raw) if path.suffix == '.gz' else raw)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def main():
    count = 0
    docs = [*ROOT.rglob('*.md'), ROOT.parent / 'README.md', ROOT.parent / 'README_en.md']
    for p in docs:
        if any(s in p.relative_to(ROOT.parent).parts for s in ['out', 'build', '_soccer_box']):
            continue
        text = p.read_text(encoding='utf-8-sig')
        for target in re.findall(r'!?\[[^\]\n]*\]\(([^\n)]+)\)', text):
            target = target.strip().split(' "', 1)[0].strip('<>')
            parsed = urlsplit(target)
            if parsed.scheme or parsed.netloc or not parsed.path:
                continue
            dest = (p.parent / unquote(parsed.path)).resolve()
            require(dest.exists(), f'链接不存在：{p.relative_to(ROOT.parent)} -> {target}')
            count += 1
    manifest = read(ROOT / 'source_manifest.json')
    for rel, info in manifest.items():
        p = ROOT / rel
        require(p.is_file(), f'缺少归档文件：{rel}')
        require(hashlib.sha256(p.read_bytes()).hexdigest() == info['sha256'], f'哈希不符：{rel}')
        if not info['modified']:
            require(info['original_sha256'] == info['sha256'], f'原样声明不符：{rel}')
    games_by_phase = {}
    total = 0
    for phase, expected in [('halftime', 2112), ('final', 2904), ('individual', 3264)]:
        data = read(ROOT / 'results' / phase / 'results.json.gz')
        schedule = read(ROOT / 'tournament' / (phase + '.json.gz'))
        games = {g['id']: g for g in data['games']}
        require(len(games) == len(data['games']) == expected, f'{phase} 局数或去重失败')
        require(set(games) == {g['id'] for g in schedule['games']}, f'{phase} 赛程局号不符')
        for planned in schedule['games']:
            g = games[planned['id']]
            for key in ['A', 'B', 'seed', 'k', 'first_kick', 'order']:
                require(g[key] == planned[key], f'{phase}/{g["id"]} {key} 不符')
            require(g['seed'] == data['seeds'][g['k']], '种子序号映射错误')
            require(g['order'] == (['b', 'a'] if g['k'] % 2 else ['a', 'b']), '座位轮换错误')
            seats = [side + '/' + slot for side in [g['A'], g['B']] for slot in g['order']]
            require(g['seats'] == seats, f'{phase}/{g["id"]} 实际座位不符')
            require(not any(v.get('status') == 'start_fail' for v in g['faults'].values()), '发现启动失败局')
        recalculated = standings(data)
        for actual, stored in zip(recalculated, data['standings']):
            for key in ['team', 'rank', 'pts', 'w', 'd', 'l', 'gf', 'ga', 'gd']:
                require(actual[key] == stored[key], f'{phase} {key} 复算不符')
        require(read(ROOT / 'results' / phase / 'programs.json') == data['programs'], '程序索引不符')
        for seat, program in data['programs'].items():
            src = ROOT / program['src']
            require(src.is_file(), f'源码不存在：{program["src"]}')
            require(hashlib.sha256(src.read_bytes()).hexdigest() == program['src_sha256'], f'{phase}/{seat} 上场源码哈希不符')
            require(schedule['programs'][seat] == program['sha256'], f'{phase}/{seat} 冻结可执行文件哈希不符')
        games_by_phase[phase] = games
        total += len(games)
    for selected in read(ROOT / 'replays' / 'selection.json'):
        g = games_by_phase[selected['phase']][selected['id']]
        require(selected['score'] == g['score'] and selected['seats'] == g['seats'], '回放清单不符')
        require((ROOT / selected['file']).is_file(), '缺少抽样回放')
    print(f'通过：{count} 个相对链接，{len(manifest)} 个归档哈希，{total} 局赛程与积分，全部上场源码和 18 局抽样回放索引。')


if __name__ == '__main__':
    main()
