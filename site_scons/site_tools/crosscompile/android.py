# -*- coding: utf-8 -*-
#
# DragonScript
#
# Copyright (C) 2017, Plüss Roland ( roland@rptd.ch )
#
# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License 
# as published by the Free Software Foundation; either 
# version 2 of the License, or (at your option) any later 
# version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

import os
import platform

from SCons.Variables import Variables, PathVariable, EnumVariable
from SCons.Script import ARGUMENTS

def generate(env, configGroup):
	ndkroot = ''
	if 'NDK_ROOT' in os.environ:
		ndkroot = os.path.expanduser(os.environ['NDK_ROOT'])
	
	# add parameters to configurate toolchain
	params = Variables(env['PARAMETER_SOURCE'], ARGUMENTS)
	params.Add(PathVariable('android_ndkroot',
		'Path to Android NDK toolchain (NDK_ROOT env-param by default)',
		'', PathVariable.PathAccept))
	params.Add(EnumVariable('android_arch', 'Android architecture to build for',
		'armv8', ['armv7','armv8','x86','x86_64'] ) )
	params.Add(('android_apilevel', 'Android API level', '26'))
	params.Update(env)
	
	try:
		configReport = env.configReport
		configReport.add('Path to Android NDK toolchain', 'android_ndkroot', configGroup)
		configReport.add('Android architecture to build for', 'android_arch', configGroup)
		configReport.add('Android API level', 'android_apilevel', configGroup)
	except:
		pass
	
	# get configuration parameters
	ndkroot = env.subst(env['android_ndkroot'])
	arch = env['android_arch']
	apilevel = env['android_apilevel']
	sysplat = platform.system().lower()
	sysarch = platform.machine().lower()
	
	# set construction variables
	#stlsupport = 'gnu-libstdc++'
	
	if arch == 'armv7':
		compiler = 'armv7a-{}-androideabi{}'.format(sysplat, apilevel)
		compiler2 = 'arm-{}-androideabi'.format(sysplat)
		arch = 'arm'
		host = 'arm-{}'.format(sysplat)
		abi = 'armeabi-v7a'
		jnidir = 'armeabi-v7a'

	elif arch == 'armv8':
		compiler = 'aarch64-{}-android{}'.format(sysplat, apilevel)
		compiler2 = 'aarch64-{}-android'.format(sysplat)
		arch = 'aarch'
		host = 'aarch64-{}'.format(sysplat)
		abi = 'arm64-v8a'
		jnidir = 'arm64-v8a'

	elif arch == 'x86':
		compiler = 'i686-{}-androideabi{}'.format(sysplat, apilevel)
		compiler2 = 'i686-{}-androideabi'.format(sysplat)
		arch = 'x86'
		host = 'i686-{}'.format(sysplat)
		abi = 'x86'
		jnidir = 'x86'

	elif arch == 'x86_64':
		compiler = 'x86_64-{}-androideabi{}'.format(sysplat, apilevel)
		compiler2 = 'x86_64-{}-androideabi'.format(sysplat)
		arch = 'x86_64'
		host = 'x86_64-{}'.format(sysplat)
		abi = 'x86_64'
		jnidir = 'x86_64'

	else:
		raise Exception('Invalid architecture {}'.format(arch))
	
	pathToolchain = os.path.join(ndkroot, 'toolchains', 'llvm', 'prebuilt', 'linux-x86_64')
	pathToolchain = os.path.join(ndkroot, 'toolchains', 'llvm', 'prebuilt', '-'.join([sysplat, sysarch]))
	pathBin = os.path.join(pathToolchain, 'bin')
	pathCompiler = os.path.join(pathBin, compiler)
	pathCompiler2 = os.path.join(pathBin, compiler2)
	cmakeToolchain = os.path.join(ndkroot, 'build', 'cmake', 'android.toolchain.cmake')
	
	env['CC'] = '{}-clang'.format(pathCompiler)
	env['CXX'] = '{}-clang++'.format(pathCompiler)
	env['LD'] = '{}-ld'.format(pathCompiler2)
	env['STRIP'] = '{}-strip'.format(pathCompiler2)
	env['OBJCOPY'] = '{}-objcopy'.format(pathCompiler2)
	env['AS'] = '{}-as'.format(pathCompiler2)
	env['AR'] = '{}-ar'.format(pathCompiler2)
	env['RANLIB'] = '{}-ranlib'.format(pathCompiler2)
	env['NASM'] = '{}/yasm'.format(pathBin)
	
	# newer NDK changed compiler file names. how lovely U_U
	if env.Detect(os.path.join(pathBin, 'llvm-ar')):
		pathCompiler2 = os.path.join(pathBin, 'llvm')
		env['LD'] = '{}-link'.format(pathCompiler2)
		env['STRIP'] = '{}-strip'.format(pathCompiler2)
		env['OBJCOPY'] = '{}-objcopy'.format(pathCompiler2)
		env['AS'] = '{}-as'.format(pathCompiler2)
		env['AR'] = '{}-ar'.format(pathCompiler2)
		env['RANLIB'] = '{}-ranlib'.format(pathCompiler2)
	
	#env['ANDROID_SYSROOT'] = pathSysroot
	env['ANDROID_BIN'] = pathBin
	env['ANDROID_COMPILER'] = compiler
	env['ANDROID_HOST'] = host
	env['ANDROID_JNIDIR'] = jnidir
	env['ANDROID_ARCH'] = arch
	env['ANDROID_ABI'] = abi
	env['ANDROID_APILEVEL'] = apilevel
	env['ANDROID_CMAKETOOLCHAIN'] = cmakeToolchain
	env['ANDROID_NDKROOT'] = ndkroot
	
	env.Append(ANDROID_LIBS = ['m', 'z', 'log', 'c', 'android'])
	
	env.Append(CPPFLAGS = ['-DANDROID={}'.format(apilevel)])
	env.Append(CPPFLAGS = ['-O3'])
	
	env['CROSSCOMPILE_HOST'] = host
	env['CROSSCOMPILE_CFLAGS'] = []
	env['CROSSCOMPILE_CPPFLAGS'] = []
	env['CROSSCOMPILE_CXXFLAGS'] = []
	env['CROSSCOMPILE_LINKFLAGS'] = []
	
	env.Append(CFLAGS = env['CROSSCOMPILE_CFLAGS'])
	env.Append(CPPFLAGS = env['CROSSCOMPILE_CPPFLAGS'])
	env.Append(CXXFLAGS = env['CROSSCOMPILE_CXXFLAGS'])
	env.Append(LINKFLAGS = env['CROSSCOMPILE_LINKFLAGS'])

def exists(env):
	return os.path.exists(env.subst(env['CC']))
