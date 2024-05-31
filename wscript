#! /usr/bin/env python
# encoding: utf-8
# Sylvain Cetre and David Barr
import os,glob
# the following two variables are used by the target "waf dist"
VERSION='0.0.1'
APPNAME='daoBase'

# if you want to cross-compile, use a different command line:
# CC=mingw-gcc AR=mingw-ar waf configure build

top = '.'
build = 'build'
include = 'include'
docs = 'docs'

from waflib import Configure, Logs, Utils, Context, Task
import subprocess
#Configure.autoconfig = True # True/False/'clobber'
from waflib.Tools import waf_unit_test

def build_docs(conf):
	os.system("doxygen docs/Doxyfile")
	os.system(f"make -C {docs} html")

def clean_docs(conf):
	os.system(f"make -C {docs} clean")

def options(opt):
	opt.load('cxx compiler_c compiler_cxx gnu_dirs waf_unit_test')
	
def configure(conf):
	conf.load('cxx compiler_c compiler_cxx gnu_dirs waf_unit_test')
	conf.write_config_header('config.h')

	# set required libs 
	conf.check_cfg( package='libzmq',
					args='--cflags --libs',
					uselib_store='ZMQ'
					)

	conf.check_cfg( package='protobuf',
					args='--cflags --libs',
					uselib_store='PROTOBUF'
					)

def build(bld):
	bld.env.DEFINES=['WAF=1']
	bld.recurse('proto')
	bld.recurse('src')# test')

	# install header files
	header_files = glob.glob('build/*.h')
	for file in header_files:
		bld.install_files(bld.env.PREFIX+'/include', file, relative_trick=False)

    # install python files
	python_files = glob.glob('build/*.py')
	for file in python_files:
		bld.install_files(bld.env.PREFIX+'/python', file, relative_trick=False)
	
	# include
	files = glob.glob('include/*.h')
	for file in files:
		bld.install_files(bld.env.PREFIX+'/include', file, relative_trick=False)
	files = glob.glob('include/*.hpp')
	for file in files:
		bld.install_files(bld.env.PREFIX+'/include', file, relative_trick=False)
	# src
	files = glob.glob('src/python/*.py')
	for file in files:
		bld.install_files(bld.env.PREFIX+'/python', file, relative_trick=False)
	files = glob.glob('src/julia/*.jl')
	for file in files:
		bld.install_files(bld.env.PREFIX+'/julia', file, relative_trick=False)
	# apps
	files = glob.glob('apps/*.py')
	for file in files:
		bld.install_files(bld.env.PREFIX+'/bin', file, chmod=0o0755, relative_trick=False)
	# gui
	files = glob.glob('gui/*.ui')
	for file in files:
		bld.install_files(bld.env.PREFIX+'/data', file, relative_trick=False)
	files = glob.glob('gui/*.py')
	for file in files:
		bld.install_files(bld.env.PREFIX+'/bin', file, chmod=0o0755, relative_trick=False)
	# script
	files = glob.glob('scripts/*')
	for file in files:
		bld.install_files(bld.env.PREFIX+'/bin', file, chmod=0o0755, relative_trick=False)
	
	# uncommment to run tests
#	bld(features='test', source='test/daoBaseTestEvent.py', target='daoBaseTestEvent', 
#        exec_command='python -m pytest ${SRC}')
#	bld.recurse('test')
#	bld.add_post_fun(waf_unit_test.summary)
