"""Correlate xperf CSwitch/ReadyThread exports with explicit ETW-us intervals.

RUN and off-CPU are measured intervals, not sampled exposure. The process that
executes ReadyThread is signal context, not proof of a lock owner or IPC peer.
Require a complete, loss-checked ETL upstream and export padding around frames.
"""
import argparse
import csv
import json
import re
from pathlib import Path


def analyze(rows, frames, pid, tid):
    headers = {}
    events = []
    for index, row in enumerate(rows):
        row = [value.strip() for value in row]
        if not row or row[0] not in ('CSwitch', 'ReadyThread'):
            continue
        if len(row) > 1 and row[1] == 'TimeStamp':
            headers[row[0]] = row
        elif len(row) > 1 and row[1].isdigit():
            events.append((int(row[1]), index, row))
    required = {
        'CSwitch': ('New Process Name ( PID)', 'New TID',
                    'Old Process Name ( PID)', 'Old TID', 'OldState', 'Wait Reason'),
        'ReadyThread': ('Process Name ( PID)', 'ThreadID',
                        'Rdy Process Name ( PID)', 'Rdy TID'),
    }
    for kind, fields in required.items():
        if kind not in headers or any(field not in headers[kind] for field in fields):
            raise ValueError('Unsupported or missing xperf header: ' + kind)
    events.sort()
    run = pending = ready = None
    runs, gaps = [], []

    def belongs(process, thread):
        match = re.search(r'\(\s*(\d+)\)$', process)
        return match is not None and int(match[1]) == pid and int(thread) == tid

    for timestamp, _, row in events:
        kind = row[0]
        if len(row) != len(headers[kind]):
            raise ValueError('Truncated xperf row')
        event = dict(zip(headers[kind], row))
        if kind == 'ReadyThread':
            if pending is not None and ready is None and belongs(
                    event['Rdy Process Name ( PID)'], event['Rdy TID']):
                ready = (timestamp, event['Process Name ( PID)'], event['ThreadID'])
            continue
        if belongs(event['Old Process Name ( PID)'], event['Old TID']):
            if run is not None:
                runs.append((run, timestamp))
            pending = (timestamp, event['OldState'], event['Wait Reason'])
            run = ready = None
        if belongs(event['New Process Name ( PID)'], event['New TID']):
            if pending is not None:
                begin, state, reason = pending
                gaps.append(dict(begin=begin, end=timestamp, state=state,
                                 reason=reason, ready=ready))
            run = timestamp
            pending = ready = None

    def overlap(a, b, start, stop):
        return max(0, min(b, stop) - max(a, start))

    results = []
    for frame in frames:
        start, stop = frame['start_us'], frame['stop_us']
        if not 0 <= start < stop:
            raise ValueError('Invalid ETW-us frame bounds')
        running = sum(overlap(a, b, start, stop) for a, b in runs)
        parts = []
        for gap in gaps:
            duration = overlap(gap['begin'], gap['end'], start, stop)
            if duration <= 0:
                continue
            ready_event = gap['ready']
            ready_at = ready_event[0] if ready_event else (
                gap['begin'] if gap['state'] == 'Ready' else None)
            if ready_at is not None and not gap['begin'] <= ready_at <= gap['end']:
                raise ValueError('Ready event outside off-CPU interval')
            parts.append(dict(out_us=gap['begin'], in_us=gap['end'],
                              overlap_us=duration, state=gap['state'], reason=gap['reason'],
                              blocked_us=(overlap(gap['begin'], ready_at, start, stop)
                                          if ready_at is not None else None),
                              ready_us=(overlap(ready_at, gap['end'], start, stop)
                                        if ready_at is not None else None),
                              ready_context=ready_event[1:] if ready_event else None))
        off = sum(part['overlap_us'] for part in parts)
        covered = running + off
        if covered > stop - start + 1:
            raise ValueError('Overlapping scheduler intervals')
        results.append(dict(frame=frame, run_us=running, off_cpu_us=off,
                            unknown_us=max(0, stop-start-covered),
                            complete=abs(covered-(stop-start)) < 1,
                            gaps=sorted(parts, key=lambda part: part['overlap_us'], reverse=True)))
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('events', type=Path)
    parser.add_argument('frames', type=Path, help='JSON list of start_us/stop_us in ETW microseconds')
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--tid', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    with args.events.open(newline='') as stream:
        result = analyze(csv.reader(stream), json.loads(args.frames.read_bytes()), args.pid, args.tid)
    args.output.write_text(json.dumps(dict(pid=args.pid, tid=args.tid,
        warning='Upstream ETL completeness/loss must be checked; ReadyThread context is not a lock owner',
        frames=result), indent=2))
    print('Analyzed', len(result), 'intervals; complete:', sum(row['complete'] for row in result))


if __name__ == '__main__':
    main()
