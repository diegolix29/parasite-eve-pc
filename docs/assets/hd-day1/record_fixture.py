#!/usr/bin/env python3
from pathlib import Path
import json,os,sys
R=Path(__file__).resolve().parent;exe=R.parents[2]/'build/parasite-eve-pc';args=[str(exe)]+sys.argv[1:];(R/'current-fixture-args.json').write_text(json.dumps(args,indent=2));os.execv(str(exe),args)
