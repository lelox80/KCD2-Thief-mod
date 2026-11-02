"""Persistent project-local REA MCP session over stdio; retain complete responses privately.

Send JSON lines containing name, arguments and optional label. Send {"quit":true} to close.
All downloaded code is run through the explicitly configured REA entry point; games are not executed.
"""
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--session', required=True)
    opts = parser.parse_args()
    if not opts.session.replace('-', '').replace('_', '').isalnum():
        raise ValueError('Use an alphanumeric session label')
    config = json.loads((ROOT / '.local/rea-toolchain.json').read_text(encoding='utf-8'))
    folder = ROOT / '.local/rea-evidence' / opts.session
    folder.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.update(config['environment'])
    sequence = 0
    with (folder / 'server-stderr.log').open('a', encoding='utf-8') as stderr:
        child = subprocess.Popen([config['node'], config['entry'], '--mcp'], cwd=ROOT,
                                 env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=stderr, text=True, encoding='utf-8', bufsize=1)
        def send(value):
            child.stdin.write(json.dumps(value) + '\n')
            child.stdin.flush()
        def request(method, params):
            nonlocal sequence
            sequence += 1
            request_id = sequence
            send({'jsonrpc': '2.0', 'id': request_id, 'method': method, 'params': params})
            while True:
                line = child.stdout.readline()
                if not line:
                    raise RuntimeError('REA server exited; inspect server-stderr.log')
                response = json.loads(line)
                if response.get('id') == request_id:
                    return response
                with (folder / 'notifications.jsonl').open('a', encoding='utf-8') as output:
                    output.write(json.dumps(response) + '\n')
        try:
            initialized = request('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {},
                'clientInfo': {'name': 'kcd2-project-research', 'version': '1.0.0'}})
            if 'error' in initialized:
                raise RuntimeError(str(initialized['error']))
            send({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
            tools = request('tools/list', {})
            (folder / 'tools.json').write_text(json.dumps(tools, indent=2), encoding='utf-8')
            print(json.dumps({'ready': True, 'session': opts.session, 'tools': str(folder / 'tools.json')}), flush=True)
            for line in sys.stdin:
                command = json.loads(line)
                if command.get('quit'):
                    break
                name = command['name']
                label = command.get('label', name)
                if not label.replace('-', '').replace('_', '').isalnum():
                    raise ValueError('Invalid evidence label')
                response = request('tools/call', {'name': name, 'arguments': command.get('arguments', {})})
                stamp = dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
                path = folder / f'{stamp}-{label}.json'
                path.write_text(json.dumps({'request': command, 'response': response}, indent=2), encoding='utf-8')
                result = response.get('result', {})
                preview = json.dumps(result.get('structuredContent', result), ensure_ascii=True)
                print(json.dumps({'saved': str(path), 'rpc_error': response.get('error'),
                                  'is_error': result.get('isError', False),
                                  'preview': preview[:command.get('preview_chars', 2500)]}), flush=True)
        finally:
            # Explicitly close the provider so its private temporary PE/project
            # can be removed; EOF alone need not run native-provider cleanup.
            if child.poll() is None:
                try:
                    request('tools/call', {'name': 'close_binary', 'arguments': {}})
                except (OSError, RuntimeError, ValueError):
                    pass
            child.stdin.close()
            try:
                child.wait(timeout=20)
            except subprocess.TimeoutExpired:
                child.terminate()
                child.wait(timeout=10)


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    main()
