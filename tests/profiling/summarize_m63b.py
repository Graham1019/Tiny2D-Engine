"""Run from repo root after the two benchmark CSVs have been captured."""
import csv
from pathlib import Path
root = Path('docs/data/m63b')
names = ['Lv.1', 'Lv.3', 'Lv.5', '48 enemies + 64 projectiles', '8 kills per tick', 'Victory', 'GameOver', 'Restart', 'Menu']
metrics = ['fixed_us','scene_proxy_us','vfx_update_us','vfx_present_us','hud_us','world_us','ui_us','frame_cpu_us','finish_wait_us','frame_completed_us']
data = {name: list(csv.DictReader((root / (name+'.csv')).open())) for name in ['baseline','polish']}
def quantile(rows,key,p):
    values=sorted(float(r[key]) for r in rows)
    return values[int((len(values)-1)*p)]
text = '# M6.3b measured distributions\n\nAll timing values are microseconds; cells are p50 / p95 / p99.\n\n'
for load,name in enumerate(names):
    text += '## '+name+'\n\n|Metric|M6.3a|M6.3b|\n|---|---|---|\n'
    rows = {k:[r for r in v if int(r['workload'])==load] for k,v in data.items()}
    for metric in metrics:
        text += '|'+metric+'|'+'|'.join(' / '.join(f'{quantile(rows[k],metric,p):.3f}' for p in [.5,.95,.99]) for k in rows)+'|\n'
    ratio=quantile(rows['polish'],'frame_cpu_us',.95)/quantile(rows['baseline'],'frame_cpu_us',.95)-1
    text+=f'\nCPU frame p95 change: {ratio:+.1%}.\n\n'
    text+='|Build|World/UI draws max|Sprites max|Flushes/binds max|Upload bytes max|Enemies/projectiles max|VFX max|Gameplay objects max|\n|---|---|---|---|---|---|---|---|\n'
    for k,rs in rows.items():
        def peak(key): return max(int(r[key]) for r in rs)
        text+=f"|{k}|{peak('world_draws')}/{peak('ui_draws')}|{peak('sprites')}|{peak('flushes')}/{peak('binds')}|{peak('bytes')}|{peak('enemies')}/{peak('projectiles')}|{peak('vfx')}|{peak('objects')}|\n"
    text+=f"\nCompleted offscreen throughput (1e6 / median frame_completed_us): {1e6/quantile(rows['polish'],'frame_completed_us',.5):.1f} frames/s. This excludes display swap/vsync and is not interactive FPS.\n\n"
(root/'summary.md').write_text(text)
