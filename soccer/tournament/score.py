"""从公开逐局数据复算胜平负、积分和 BT；不生成团队/个人合并总分。"""
import argparse
import json
import bt
from reproduce import read


def standings(data):
    ids = {r['team'] for r in data['standings']}
    table = {n: dict(team=n, pts=0, w=0, d=0, l=0, gf=0, ga=0) for n in ids}
    for g in data['games']:
        if g['A'] not in ids or g['B'] not in ids:
            continue
        for a, b, gf, ga in [(g['A'], g['B'], *g['score']), (g['B'], g['A'], *g['score'][::-1])]:
            r = table[a]
            outcome = 'w' if gf > ga else 'd' if gf == ga else 'l'
            r[outcome] += 1
            r['pts'] += {'w': 3, 'd': 1, 'l': 0}[outcome]
            r['gf'] += gf
            r['ga'] += ga
    for r in table.values():
        r['gd'] = r['gf'] - r['ga']
        r['rate'] = r['pts'] / (3 * (r['w'] + r['d'] + r['l']))
    rows = sorted(table.values(), key=lambda r: (-r['rate'], -r['gd'], -r['gf'], r['team']))
    for rank, r in enumerate(rows, 1):
        r['rank'] = rank
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('phase', choices=['halftime', 'final', 'individual'])
    ap.add_argument('--bt', action='store_true', help='同时复算 BT 及 95%% 区间（300 次按种子重抽样）')
    args = ap.parse_args()
    data = read(args.phase)
    result = {'standings': standings(data)}
    if args.bt:
        games = [{'map': g['seed'], 'a': g['A'], 'b': g['B'],
                  'score': 1 if g['score'][0] > g['score'][1] else .5 if g['score'][0] == g['score'][1] else 0}
                 for g in data['games']]
        names = [r['team'] for r in data['standings']] + [r['id'] for r in data['references']]
        result['bt'], _ = bt.rate(games, names, 'ref-baseline', boot=300)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
