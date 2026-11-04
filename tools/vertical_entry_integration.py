#!/usr/bin/env python3
"""Run entry integration against an already prepared disposable creation fixture."""
import argparse
from pathlib import Path
import subprocess
from parallel_lock import resource_lock

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('catalog', help='Existing Eternity_Test_Creation_ fixture catalog')
args = parser.parse_args()
if not args.catalog.startswith('Eternity_Test_Creation_') or not args.catalog.replace('_', '').isalnum():
    parser.error('Expected an existing disposable creation fixture catalog')
with resource_lock(root, 'sql'):
    subprocess.run(['docker', 'run', '--rm', '--mount', f'type=bind,source={root},target=/src',
                    '--network', 'container:eternity-sqlserver', '--env-file', str(root / '.local/runtime.env'),
                    'eternity-dev:bookworm', 'build/vertical-entry-sql-integration', args.catalog],
                   cwd=root, check=True)
