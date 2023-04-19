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

from waflib import Configure, Logs, Utils, Context
#Configure.autoconfig = True # True/False/'clobber'
from waflib.Tools import waf_unit_test

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

from waflib.Task import Task
class proto_cpp(Task):
    def run(self):
        return self.exec_command(f'protoc -I{self.inputs[0]} --cpp_out={self.outputs[0]} {self.inputs[1]}')

class proto_py(Task):
    def run(self):
        return self.exec_command(f'protoc -I{self.inputs[0]} --python_out={self.outputs[0]} {self.inputs[1]}')

def build_proto_list(bld, list, proto_path='../proto/', build_path=''):
    for i in list:
        cpp = proto_cpp(env=bld.env)
        cpp.set_inputs([bld.path.find_or_declare(proto_path),bld.path.find_or_declare(proto_path + i)])
        cpp.set_outputs(bld.path.find_or_declare(build_path))
        bld.add_to_group(cpp) 

        py = proto_py(env=bld.env)
        py.set_inputs([bld.path.find_or_declare(proto_path),bld.path.find_or_declare(proto_path + i)])
        py.set_outputs(bld.path.find_or_declare(build_path))
        bld.add_to_group(py)
    bld.execute_build()
    return bld

def build(bld):
	bld = build_proto_list(bld,['daoCommand.proto', 'daoLogging.proto', 'daoEvent.proto'])
	bld.shlib(
        target='daoBase',
        source= ["build/daoCommand.pb.cc", "build/daoLogging.pb.cc", "build/daoEvent.pb.cc"],
        cppflags=['-std=c++11'],
        use=['PROTOBUF']
    )

	bld.env.DEFINES=['WAF=1']
	bld.recurse('src')
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