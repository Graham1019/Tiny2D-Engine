#!/usr/bin/env python3
"""Read-only fail-closed validation; never launches a graphical application."""
import pathlib, plistlib, subprocess, sys
app = pathlib.Path(sys.argv[1]).resolve()
contents = app / 'Contents'
def run(*args):
    return subprocess.check_output(args, text=True)
def require(condition, message):
    if not condition:
        raise SystemExit(message)
with (contents / 'Info.plist').open('rb') as f:
    info = plistlib.load(f)
for key, expected in {'CFBundleExecutable':'Tiny2DSurvivor', 'CFBundleName':'Tiny2D Survivor',
                      'CFBundleDisplayName':'Tiny2D Survivor', 'CFBundlePackageType':'APPL',
                      'CFBundleIdentifier':'io.github.graham1019.tiny2d.survivor',
                      'CFBundleShortVersionString':'0.5.0', 'CFBundleVersion':'5001'}.items():
    require(info.get(key) == expected, f'Invalid {key}')
exe = contents / 'MacOS' / info['CFBundleExecutable']
require(exe.is_file() and exe.stat().st_mode & 0o111, 'Missing executable')
root = pathlib.Path(__file__).resolve().parent.parent
for source in (root / 'sandbox/assets').rglob('*'):
    if source.is_file():
        target = contents / 'Resources/assets' / source.relative_to(root / 'sandbox/assets')
        require(target.is_file() and target.read_bytes() == source.read_bytes(), f'Asset mismatch: {target}')
for source in (root / 'docs/third-party').glob('*LICENSE*'):
    target = contents / 'Resources/licenses' / source.name
    require(target.is_file() and target.read_bytes() == source.read_bytes(), f'License mismatch: {target}')
version = lambda text: tuple(int(x) for x in text.split('.'))
for binary in [exe, *contents.rglob('*.dylib')]:
    require(run('lipo','-archs',str(binary)).strip() == 'arm64', f'Not arm64 only: {binary}')
    load = run('otool','-l',str(binary)).splitlines()
    for line in load:
        if line.strip().startswith('minos '):
            require(version(line.split()[1]) <= version(info['LSMinimumSystemVersion']), 'Dependency deployment target exceeds plist')
        if line.strip().startswith('path ') and ' (offset ' in line:
            require('/opt/homebrew' not in line and '/Users/' not in line, f'External rpath: {line}')
    dependencies = run('otool','-L',str(binary))
    print(dependencies)
    for line in dependencies.splitlines()[1:]:
        dep = line.strip().split(' (compatibility')[0]
        if dep.startswith(('/System/Library/', '/usr/lib/')):
            continue
        resolved = dep.replace('@executable_path', str(exe.parent)).replace('@loader_path',str(binary.parent))
        target = pathlib.Path(resolved).resolve()
        require(target.is_relative_to(contents) and target.is_file(), f'External or unresolved dependency: {dep}')
print('Bundle structure, assets, architecture, deployment and dependency closure PASS')
