# -*- coding: utf-8 -*-

import os
import fnmatch
import tarfile

import SCons

# glob files in directory relative to SConscript and adds all files matching
# pattern to result. the added files have directory prefixed.
# 
# search: Directory under SConscript directory to search
# pattern: Pattern to match files against
# result: List to add found files
# recursive: Search in sub-directories
def globFiles(env, search, pattern, result, recursive=True):
	oldcwd = os.getcwd()
	os.chdir(env.Dir('.').srcnode().abspath)
	
	for root, dirs, files in os.walk(search):
		if recursive:
			if '.svn' in dirs:
				dirs.remove('.svn')
			if '.git' in dirs:
				dirs.remove('.git')
		else:
			del dirs[:]
		
		for s in fnmatch.filter(files, pattern):
			result.append(root + os.sep + s)
	
	os.chdir(oldcwd)

# umask safe untar. the tarfile module in python is bugged not respecting umask.
# under certain conditions this can lead to problems. to avoid these prolems
# this version of untar first unpacks the archive then walks over all unpacked
# files fixing the file permissions. this is done by masking with 0077 which is
# guaranteed to never yield troubles even under specific situations
def untarArchive(target, source, umask=0o077):
	mask = ~umask
	tf = tarfile.open(source, 'r')
	tf.extractall(target)
	for m in tf.getmembers():
		os.chmod(os.path.join(target, m.name), m.mode & mask)
	tf.close()

# ternary variable
_TernaryVariableStringsYes = ('y', 'yes', 'true', 't', '1', 'on', 'all')
_TernaryVariableStringsNo = ('n', 'no', 'false', 'f', '0', 'off', 'none')
_TernaryVariableStringsAuto = ('auto')

TernaryVariableYes = 'yes'
TernaryVariableNo = 'no'
TernaryVariableAuto = 'auto'

def _TernaryVariableMapper(text):
	textLower = str(text).lower()
	if textLower in _TernaryVariableStringsYes:
		return TernaryVariableYes
	if textLower in _TernaryVariableStringsNo:
		return TernaryVariableNo
	if textLower in _TernaryVariableStringsAuto:
		return TernaryVariableAuto
	raise ValueError('Invalid value for ternary option: %s' % text)

def _TernaryVariableValidator(key, value, env):
	if not env[key] in (TernaryVariableYes, TernaryVariableNo, TernaryVariableAuto):
		raise SCons.Errors.UserError('Invalid value for ternary option %s: %s' % (key, env[key]))

def TernaryVariable(key, help, default=TernaryVariableAuto):
	return (key, '%s (yes|no|auto)' % help, default, _TernaryVariableValidator, _TernaryVariableMapper)

def StringVariable(key, help, default=''):
	return (key, '%s (string)' % help, default)
