#! /usr/bin/env python
# encoding: utf-8
# Sylvain Cetre and David Barr
import os,glob,re,sys
import shutil
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
	

def copy_and_replace(src_file, old_string, new_string, dst_file=None):
	if dst_file is None:
		dst_file = src_file
	# Read the contents of the source file and replace the desired string
	with open(src_file, "r") as sources:
		lines = sources.readlines()
	with open(dst_file, "w") as sources:
		for line in lines:
			sources.write(re.sub(old_string, new_string, line))

def is_line_present(filename, line):
    with open(filename, "r") as file:
        for existing_line in file:
            if existing_line.strip() == line.strip():
                return True
    return False

def configure_enviroment(conf):
	folder_name = os.path.basename(conf.env.PREFIX)
	folder_path  = os.path.dirname(conf.env.PREFIX)

	daoroot = folder_path + "/" + folder_name
	daodata = folder_path + "/" + "DAODATA"

	copy_and_replace('templates/dao.env', '_daoroot_', daoroot,'build/dao.env')
	copy_and_replace('build/dao.env', '_daodata_', daodata)

	env_file = os.path.join(os.path.expanduser("~"), ".dao/dao.env")
	python_path = sys.executable
	copy_and_replace('templates/daoLogServer.service', '_daoroot_', daoroot,'build/daoLogServer.service')
	copy_and_replace('build/daoLogServer.service', '_envfile_', env_file)
	copy_and_replace('build/daoLogServer.service', '_python_', python_path)

	log_location = os.path.join(os.path.expanduser("~"), ".dao/logs")
	copy_and_replace('templates/daoLogServer.conf', '_loglocation_', log_location,'build/daoLogServer.conf')
	
	bashrc_path = os.path.expanduser("~/.bashrc")
	new_line = f"source {env_file}\n"
	if not is_line_present(bashrc_path, new_line):
		with open(bashrc_path, "a") as bashrc:
			bashrc.write('\n')
			bashrc.write(new_line)
		print("Line added to .bashrc.")
	else:
		print("Line is already present in .bashrc.")
	
	return daoroot, daodata

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
	
	# apps
	files = glob.glob('apps/*.py')
	for file in files:
		bld.install_files(bld.env.PREFIX+'/bin', file, chmod=0o0755, relative_trick=False)

	daoroot, daodata = configure_enviroment(bld)
	# install config files
	config_files = glob.glob('build/*.conf')
	for file in config_files:
		bld.install_files(daodata+'/config', file, relative_trick=False)

	# install service files
	destination_dir = os.path.join(os.path.expanduser("~"), ".config/systemd/user/")
	config_files = glob.glob('build/*.service')
	for file in config_files:
		bld.install_files(destination_dir, file, relative_trick=False)

	destination_dir = os.path.join(os.path.expanduser("~"), ".dao/")
	files = glob.glob('build/*.env')
	for file in files:
		bld.install_files(destination_dir, file, relative_trick=False)

# # uncommment to run tests
# #	bld(features='test', source='test/daoBaseTestEvent.py', target='daoBaseTestEvent', 
# #        exec_command='python -m pytest ${SRC}')
# #	bld.recurse('test')
# #	bld.add_post_fun(waf_unit_test.summary)