import os, sys

from string_variable import StringVariable
from ternary_variable import TernaryVariable

# create parent environment and load tools
globalEnv = Environment()
globalEnv['PARAMETER_SOURCE'] = ['custom.py']
globalEnv.Tool('config_report')
globalEnv.Tool('target_manager')
globalEnv.Tool('crosscompile')
globalEnv.Tool('build_verbose')
globalEnv.Tool('downloadArtifact')
globalEnv.Tool('runExternalCommand')
globalEnv.Tool('archive_builder')

# move to platform tool
params = Variables(globalEnv['PARAMETER_SOURCE'], ARGUMENTS)
params.Add(EnumVariable('target_platform', 'Target platform', 'auto',
	['auto', 'linux', 'windows', 'beos', 'macos', 'android']))
params.Update(globalEnv)

globalEnv.configReport.add('Target platform', 'target_platform')

targetPlatform = globalEnv['target_platform']
if targetPlatform == 'auto':
	if sys.platform == 'haiku1':
		targetPlatform = 'beos'
	elif os.name == 'win32' or sys.platform == 'win32':
		targetPlatform = 'windows'
	elif sys.platform == 'darwin':
		targetPlatform = 'macos'
	elif os.name == 'posix':
		targetPlatform = 'linux'
	else:
		raise 'Can not auto-detect platform'

globalEnv['TARGET_PLATFORM'] = targetPlatform

# parameters
params = Variables(globalEnv['PARAMETER_SOURCE'], ARGUMENTS)
params.Add(BoolVariable('with_debug', 'Build with debug symbols for GDB usage', False))
params.Add(BoolVariable('with_warnerrors', 'Treat warnings as errors (dev-builds)', False))
params.Add(BoolVariable('with_sanitize', 'Enable sanitizing (dev-builds)', False))
params.Add(BoolVariable('with_sanitize_threads', 'Enable thread sanitizing (dev-builds)', False))
params.Add(BoolVariable('with_optimizations', 'Enable run-time optimizations', True))
params.Add(StringVariable('version', 'Version', 'development'))
params.Add(BoolVariable('with_client_debug', 'Use client debug (dev-builds)', False))
params.Add(PathVariable('denetwork_includes', help='Path to DENetwork headers', default=None))
params.Add(PathVariable('denetwork_libraries', help='Path to DENetwork libraries', default=None))
params.Add(PathVariable('delauncher_includes', help='Path to DELauncher headers', default=None))
params.Add(PathVariable('delauncher_libraries', help='Path to DELauncher libraries', default=None))
params.Add(PathVariable('dragengine_includes', help='Path to Drag[en]gine headers', default=None))
params.Add(PathVariable('dragengine_libraries', help='Path to Drag[en]gine libraries', default=None))
params.Add(PathVariable('libfox_includes', help='Path to FOX-1.6 headers', default=None))
params.Add(PathVariable('libfox_libraries', help='Path to FOX-1.6 libraries', default=None))

params.Add(StringVariable('with_threads', 'Count of threads to use for building external packages', '1'))
params.Add(StringVariable('url_extern_artifacts',
	'Base URL to download external artifacts from if missing',
	'https://dragondreams.s3.eu-central-1.amazonaws.com/dragengine/extern'))
params.Add(TernaryVariable('with_system_fox', 'Use System FOX Toolkit'))

params.Update(globalEnv)

# build
SConscript(dirs='extern/fox', variant_dir='{}/build'.format('extern/fox'), duplicate=0, exports='globalEnv')
SConscript(dirs='extern/denetwork', variant_dir='{}/build'.format('extern/denetwork'), duplicate=0, exports='globalEnv')
SConscript(dirs='extern/dragengine', variant_dir='{}/build'.format('extern/dragengine'), duplicate=0, exports='globalEnv')

SConscript(dirs='shared', variant_dir='shared/build', duplicate=0, exports='globalEnv')
SConscript(dirs='launcher/desktop', variant_dir='launcher/desktop/build', duplicate=0, exports='globalEnv')

# build all target
targets = []
for t in globalEnv.targetManager.targets.values():
	try:
		targets.extend(t.build)
	except:
		pass
globalEnv.targetManager.add('build', globalEnv.targetManager.Target('Build All', globalEnv.Alias('build', targets)))

# install all target
targets = []
for t in globalEnv.targetManager.targets.values():
	try:
		targets.extend(t.install)
	except:
		pass
globalEnv.targetManager.add('install', globalEnv.targetManager.Target('Install All', globalEnv.Alias('install', targets)))

# archive all target
targets = []
for t in globalEnv.targetManager.targets.values():
	try:
		targets.extend(t.archive)
	except:
		pass
globalEnv.targetManager.add('archive', globalEnv.targetManager.Target('Archive All', globalEnv.Alias('archive', targets)))

# by default just build
Default('build')

# finish
globalEnv.targetManager.updateHelp()
print(globalEnv.configReport)
globalEnv.configReport.save()
