#! /usr/bin/env python
# encoding: utf-8
# Sylvain Cetre
import os,glob
# the following two variables are used by the target "waf dist"
VERSION='0.0.1'
APPNAME='daoBase'

# if you want to cross-compile, use a different command line:
# CC=mingw-gcc AR=mingw-ar waf configure build

top = '.'
top = '.'
build = 'build'
include = 'include'

from waflib import Configure, Logs, Utils, Context
#Configure.autoconfig = True # True/False/'clobber'

def options(opt):
	opt.load('compiler_c compiler_cxx gnu_dirs waf_unit_test')

def configure(conf):
	conf.load('compiler_c compiler_cxx gnu_dirs waf_unit_test')
	conf.write_config_header('config.h')
	print('→ prefix is ' + conf.options.prefix)

	rLibs = [['libzmq', 'ZMQ'],
			 ['protobuf','PROTOBUF']
			]
	for i in rLibs:
		conf.check_cfg( package=i[0],
		args='--cflags --libs',
		uselib_store=i[1]
		)

def build(bld):
	bld.env.DEFINES=['WAF=1']
	bld.recurse('src')

	bld.program(
		features='test',
		target = 'test_queue',
		source = [ 'test/test_queue.cpp' ],
		includes = ['include/', bld.env.PREFIX+'/include',],
		lib = [ 'gtest'],
		cxxflags=['-g', '-std=c++2a'],
		ldflags=[ '-L'+ bld.env.PREFIX+'/lib64'],
		use=['PROTOBUF', 'ZMQ']
		)

	bld.program(
		features='test',
		target = 'test_log',
		source = [ 'test/test_log.cpp' ],
		includes = ['include/', bld.env.PREFIX+'/include',],
		lib = [ 'gtest'],
		cxxflags=['-g', '-std=c++2a'],
		ldflags=[ '-L'+ bld.env.PREFIX+'/lib64', '-pthread','-ldaoProto'],
		use=['PROTOBUF', 'ZMQ']
		)

	bld.program(
		target = 'receive_stream',
		source = [ 'src/cpp/main.cpp' ],
		includes = ['include/', bld.env.PREFIX+'/include',],
		cxxflags=['-g', '-std=c++2a'],
		ldflags=[ '-L'+ bld.env.PREFIX+'/lib64', '-pthread','-ldaoProto', '-ldaoNuma', '-lnuma'],
		use=['PROTOBUF', 'ZMQ']
		)

	bld.program(
		target = 'send_stream',
		source = [ 'src/cpp/send_stream.cpp' ],
		includes = ['include/', bld.env.PREFIX+'/include',],
		cxxflags=['-g', '-std=c++2a'],
		ldflags=[ '-L'+ bld.env.PREFIX+'/lib64', '-pthread','-ldaoProto', '-ldaoNuma', '-lnuma'],
		use=['PROTOBUF', 'ZMQ']
		)


	bld.install_files(bld.env.PREFIX+'/include', 'include/daoBase.h', relative_trick=False)
	bld.install_files(bld.env.PREFIX+'/include', 'include/daoImageStruct.h', relative_trick=False)
	bld.install_files(bld.env.PREFIX+'/include', 'include/daoShmIfce.hpp', relative_trick=False)
	bld.install_files(bld.env.PREFIX+'/python', 'src/python/shmlib.py', relative_trick=False)
	
	files = glob.glob(include + '/*.hpp')
	for file in files:
		bld.install_files(bld.env.PREFIX+'/include', file, relative_trick=False)

	from waflib.Tools import waf_unit_test
	bld.add_post_fun(waf_unit_test.summary)