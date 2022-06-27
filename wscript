#! /usr/bin/env python
# encoding: utf-8
# Sylvain Cetre
import os
# the following two variables are used by the target "waf dist"
VERSION='0.0.1'
APPNAME='daoBase'

# if you want to cross-compile, use a different command line:
# CC=mingw-gcc AR=mingw-ar waf configure build

top = '.'

from waflib import Configure, Logs, Utils, Context
#Configure.autoconfig = True # True/False/'clobber'

def options(opt):
	opt.load('compiler_c gnu_dirs')

def configure(conf):
	conf.load('compiler_c gnu_dirs')
	conf.write_config_header('config.h')
	print('→ prefix is ' + conf.options.prefix)

def build(bld):
	bld.env.DEFINES=['WAF=1']
	bld.recurse('src apps')
	#bld.install_files('/tmp/foo', 'wscript')
	bld.install_files(bld.env.PREFIX+'/include', 'include/daoBase.h', relative_trick=False)
	bld.install_files(bld.env.PREFIX+'/include', 'include/daoImageStruct.h', relative_trick=False)
	bld.install_files(bld.env.PREFIX+'/python', 'src/python/shmlib.py', relative_trick=False)
	bld.install_files(bld.env.PREFIX+'/bin', 'demos/daoBaseDemoBuildShm.py', relative_trick=False)
	bld.install_files(bld.env.PREFIX+'/bin', 'demos/daoBaseStartDemo', relative_trick=False)
	bld.install_files(bld.env.PREFIX+'/bin', 'demos/daoBaseStopDemo', relative_trick=False)
#os.chmod(bld.env.PREFIX+'/bin/daoExampleBuildShm.py', 0o755)
