#!/usr/bin/env python3
"""Read-only restored SQL/original loader comparison under Eternity's shared SQL lock."""
from pathlib import Path
import subprocess
from parallel_lock import resource_lock

ROOT=Path(__file__).resolve().parents[1]


def main():
    work=ROOT/'work';work.mkdir(exist_ok=True)
    rows=work/'mob-targeting-bindings-sql-rows.tsv'
    original=work/'mob-targeting-bindings-sql-original.tsv'
    command=['docker','run','--rm','--mount',f'type=bind,source={ROOT},target=/src',
             '--network','container:eternity-sqlserver','--env-file',str(ROOT/'.local/runtime.env'),
             'eternity-dev:bookworm','build/mob-targeting-bindings-sql-unit','work/'+rows.name]
    with resource_lock(ROOT,'sql'):
        print('Read-only isolated SQL binding export',flush=True)
        subprocess.run(command,cwd=ROOT,check=True)
        print('Execute original loaders over the exact restored numeric rows',flush=True)
        with original.open('w') as output:
            subprocess.run([str(ROOT/'.local/oracle-env/bin/python'),str(ROOT/'tools/mob_targeting_bindings_oracle.py'),'--sql-rows',str(rows)],cwd=ROOT,stdout=output,check=True)
        print('Compare the complete native binding catalog with original execution',flush=True)
        subprocess.run(command+['work/'+original.name],cwd=ROOT,check=True)


if __name__=='__main__':main()
