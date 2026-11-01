"""Reject development-machine DLL dependencies in Windows release executables."""
from pathlib import Path
import subprocess
import sys

report = subprocess.check_output(['objdump', '-p', sys.argv[1]], text=True)
names = {line.split('DLL Name:', 1)[1].strip().lower()
         for line in report.splitlines() if 'DLL Name:' in line}
system = set('''advapi32.dll bcrypt.dll bcryptprimitives.dll cfgmgr32.dll comdlg32.dll
crypt32.dll d3d12.dll d3dcompiler_47.dll dinput8.dll dwmapi.dll dxgi.dll gdi32.dll
imm32.dll iphlpapi.dll kernel32.dll msvcrt.dll normaliz.dll ntdll.dll ole32.dll
oleaut32.dll opengl32.dll powrprof.dll secur32.dll setupapi.dll shell32.dll
shlwapi.dll ucrtbase.dll uiautomationcore.dll user32.dll uxtheme.dll version.dll
windowscodecs.dll winmm.dll winspool.drv wintrust.dll ws2_32.dll'''.split())
unexpected = sorted(n for n in names if n not in system and
                    not n.startswith(('api-ms-win-', 'ext-ms-win-')))
if len(sys.argv) > 2:
    Path(sys.argv[2]).write_text(report)
assert names and not unexpected, f'Unbundled Windows runtime dependencies: {unexpected}'
print(f'{sys.argv[1]} imports only operating-system libraries ({len(names)} DLLs).')
