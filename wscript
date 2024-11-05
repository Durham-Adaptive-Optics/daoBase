#! /usr/bin/env python
# encoding: utf-8
# Sylvain Cetre and David Barr
import os,glob,platform
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

from build_tools import pkg_tool
def build_docs(conf):
	os.system("doxygen docs/Doxyfile")
	os.system(f"make -C {docs} html")

def clean_docs(conf):
	os.system(f"make -C {docs} clean")

def options(opt):
	opt.load('cxx compiler_c compiler_cxx gnu_dirs waf_unit_test')
 
	opt.add_option('--debug', dest='debug_flag', default=False, action='store_true',
             help='Adds \'-g\' to compiler flags to allow for debuging')
 
	opt.add_option('--sanitizer', dest='sanitizer_flag', default=False, action='store_true',
             help='flags for address sanitsation')
 
	opt.add_option('--test', dest='test_flag', default=False, action='store_true',
             help='flags for running tests')
	
def configure(conf):
	conf.load('cxx compiler_c compiler_cxx gnu_dirs waf_unit_test')
	conf.load('build_tools.pkg_tool')
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

	# add some custom locations
	conf.env.PYTHONDIR		= f'{conf.env.PREFIX}/python'
	conf.env.DATADIR		= f'{conf.env.PREFIX}/data'
	conf.env.PKGCONFIGDIR	= f'{conf.env.LIBDIR}/pkgconfig'
	conf.env.JULIADIR		= f'{conf.env.PREFIX}/julia'
 
def build(bld):
	bld.env.DEFINES=['WAF=1']
	bld.recurse('proto')
	bld.recurse('src')# test')
 
	# install header files
	header_files = glob.glob('build/*.h')
	for file in header_files:
		bld.install_files(bld.env.INCLUDEDIR, file, relative_trick=False)
    # install python files
	python_files = glob.glob('build/*.py')
	for file in python_files:
		bld.install_files(bld.env.PYTHONDIR, file, relative_trick=False)
    # install python files
	build_files = glob.glob('build_tools/*.py')
	for file in build_files:
		bld.install_files(f'{bld.env.PYTHONDIR}/build_tools', file, relative_trick=False)
	#pkgconfig 
	pc_files = glob.glob('build/*.pc')
	for file in pc_files:
		bld.install_files(bld.env.PKGCONFIGDIR, file, relative_trick=False)
	# include
	files = glob.glob('include/*.h')
	for file in files:
		bld.install_files(bld.env.INCLUDEDIR, file, relative_trick=False)
	files = glob.glob('include/*.hpp')
	for file in files:
		bld.install_files(bld.env.INCLUDEDIR, file, relative_trick=False)
	# src
	files = glob.glob('src/python/*.py')
	for file in files:
		bld.install_files(bld.env.PYTHONDIR, file, relative_trick=False)

	files = glob.glob('src/julia/*.jl')
	for file in files:
		bld.install_files(bld.env.JULIADIR, file, relative_trick=False)
	# apps
	files = glob.glob('apps/*.py')
	for file in files:
		bld.install_files(bld.env.BINDIR, file, chmod=0o0755, relative_trick=False)
	# gui
	files = glob.glob('gui/*.ui')
	for file in files:
		bld.install_files(bld.env.DATADIR, file, relative_trick=False)
	files = glob.glob('gui/*.py')
	for file in files:
		bld.install_files(bld.env.BINDIR, file, chmod=0o0755, relative_trick=False)
	# script
	files = glob.glob('scripts/*')
	for file in files:
		bld.install_files(bld.env.BINDIR, file, chmod=0o0755, relative_trick=False)
  
	
	# uncommment to run tests
#	bld(features='test', source='test/daoBaseTestEvent.py', target='daoBaseTestEvent', 
#        exec_command='python -m pytest ${SRC}')
	if bld.options.test_flag:
		bld.recurse('test')
		bld.add_post_fun(waf_unit_test.summary)
