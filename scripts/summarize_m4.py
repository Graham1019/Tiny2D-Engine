#!/usr/bin/env python3
"""Print median-of-five trial percentiles from tiny2d_benchmark CSV output."""
import csv
import statistics
import sys

with open(sys.argv[1] if len(sys.argv) > 1 else 'trials.csv', newline='') as source:
    rows = list(csv.DictReader(source))
metrics = ['cpu_p50_us', 'cpu_p95_us', 'gpu_p50_us', 'gpu_p95_us']
print('workload,sprites,renderer,' + ','.join(metrics))
for workload in ('cpu', 'pixel'):
    for count in (100, 500, 1000, 5000):
        for renderer in ('immediate', 'batch'):
            group = [row for row in rows if row['workload'] == workload
                     and int(row['sprites']) == count and row['renderer'] == renderer]
            if len(group) != 5:
                raise ValueError(f'Expected five trials: {workload}/{count}/{renderer}')
            results = []
            for metric in metrics:
                values = [row[metric] for row in group]
                results.append('unavailable' if 'unavailable' in values else
                               f'{statistics.median(map(float, values)):.2f}')
            print(','.join([workload, str(count), renderer, *results]))
