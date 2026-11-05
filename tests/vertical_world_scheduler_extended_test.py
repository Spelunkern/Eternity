#!/usr/bin/env python3
"""Validate coverage and structural invariants of the original-execution trace."""
from pathlib import Path

fixture = Path(__file__).resolve().parent / 'data' / 'vertical-world-scheduler-extended.tsv'
intervals = (100, 1000, 3000, 5000, 500, 1000, 3000, 10000, 60000, 100)
rows = [line.split('\t') for line in fixture.read_text().splitlines() if line and not line.startswith('#')]
assert len(rows) == 2496
for row in rows:
    case, now, mask, promote, mutation, relation = map(int, row[:6])
    before = list(map(int, row[6].split(','))); after = list(map(int, row[7].split(',')))
    active_count, wait_count = map(int, row[8:10]); events = row[10].split(',')
    due = [now >= x for x in before]
    assert after == [((now + interval) & 0xffffffff) if enabled else old
                     for old, interval, enabled in zip(before, intervals, due)], case
    active = [0, 1]; expected = []
    if due[9]:
        active.extend((2, 3)); expected.extend(('promote:2', 'promote:3'))
    packets = list(active)
    if mutation == 1:
        active.remove(0)
    elif mutation == 2:
        active.remove(1);packets.remove(1)
    elif mutation == 3:
        active.append(4);packets.append(4)
    expected.extend(f'packet:{z}' for z in packets)
    for i, names in enumerate((('user_attack',), ('skill',), ('effect_tick',), ('effect',),
                               ('mob_attack','npc_attack'), ('mob_idle','trap'),
                               ('npc_idle',), ('item',), ('quest',))):
        if due[i]:
            expected.extend(f'{name}:{z}' for z in active for name in names)
    if now != 0xffffffff:
        expected.append('kill_count:thread')
        expected.extend(f'world_send:{z}' for z in active)
        if any(due):expected.extend(f'net_send:{z}' for z in active)
    assert events == expected, (case, events, expected)
    assert active_count == len(active) and wait_count == (0 if due[9] else 2), case
print(f'{len(rows)} original traces: deadlines, promotion, live-list mutation and observed pass order checked')
