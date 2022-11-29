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
#	print('→ prefix is ' + conf.options.prefix)
#
#	rLibs = [['libzmq', 'ZMQ'],
#			 ['protobuf','PROTOBUF']
#			]
#	for i in rLibs:
#		conf.check_cfg( package=i[0],
#		args='--cflags --libs',
#		uselib_store=i[1]
#		)

def build(bld):
	bld.env.DEFINES=['WAF=1']
	bld.recurse('src')
	bld.install_files(bld.env.PREFIX+'/include', 'include/daoBase.h', relative_trick=False)
	
#	from waflib.Tools import waf_unit_test
#	bld.add_post_fun(waf_unit_test.summary)