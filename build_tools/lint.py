# build_tools/lint.py
# Linting functionality for daoBase

import os
import subprocess
from waflib import Logs
import datetime

def setup_lint_configs(top='.'):
    """Set up the linting configuration files"""
    # Create directories for linting config files if they don't exist
    lint_configs_dir = os.path.join(top, 'build_tools', 'lint_configs')
    if not os.path.exists(lint_configs_dir):
        os.makedirs(lint_configs_dir)
    
    # Create clang-format config file for C/C++ with Google style but Allman brackets
    clang_format_file = os.path.join(lint_configs_dir, '.clang-format')
    if not os.path.exists(clang_format_file):
        with open(clang_format_file, 'w') as f:
            f.write("""---
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: false
AllowShortLoopsOnASingleLine: false
...
""")
    
    # Create .pylintrc for Python linting
    pylintrc_file = os.path.join(lint_configs_dir, '.pylintrc')
    if not os.path.exists(pylintrc_file):
        try:
            subprocess.run(['pylint', '--generate-rcfile'], stdout=open(pylintrc_file, 'w'), check=True)
        except (subprocess.SubprocessError, FileNotFoundError):
            Logs.warn("pylint not found, cannot generate default config file")
            with open(pylintrc_file, 'w') as f:
                f.write("""[MASTER]
init-hook='import sys; sys.path.append(".")'
disable=C0103,C0111
[FORMAT]
max-line-length=100
""")
    
    return lint_configs_dir, clang_format_file, pylintrc_file

def setup_logging(top='.'):
    """Set up logging for lint command output"""
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_dir = os.path.join(top, 'build', 'logs')
    if not os.path.exists(log_dir):
        os.makedirs(log_dir)
    log_file = os.path.join(log_dir, f'lint_{timestamp}.log')
    return log_file

def find_source_files(top='.'):
    """Find all source files that should be linted"""
    py_files = []
    c_files = []
    cpp_files = []
    
    for root, _, files in os.walk(top):
        if 'build' in root or '.git' in root:
            continue
        for file in files:
            if file.endswith('.py'):
                py_files.append(os.path.join(root, file))
            elif file.endswith(('.c', '.h')):
                c_files.append(os.path.join(root, file))
            elif file.endswith(('.cpp', '.hpp')):
                cpp_files.append(os.path.join(root, file))
    
    return py_files, c_files, cpp_files

def lint_python_files(py_files, pylintrc_file, log_file=None):
    """Run linting on Python files"""
    if py_files:
        Logs.info("Running pylint on Python files...")
        try:
            with open(log_file, 'a') if log_file else None as f:
                if f:
                    f.write("===== PYTHON LINTING =====\n")
                result = subprocess.run(['pylint', '--rcfile=' + pylintrc_file] + py_files, 
                                       check=False, 
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT,
                                       text=True)
                if f:
                    f.write(result.stdout)
                # Still print a summary to console
                Logs.info(f"Python linting completed - see details in {log_file}")
                print_summary(result.stdout)
        except FileNotFoundError:
            error_msg = "pylint not found. Install with: pip install pylint"
            Logs.error(error_msg)
            if log_file:
                with open(log_file, 'a') as f:
                    f.write(f"ERROR: {error_msg}\n")
            return False
    return True

def print_summary(output):
    """Print a short summary of the linting results"""
    # Extract rating line and errors count if available
    lines = output.splitlines()
    summary_lines = [line for line in lines if "rated at" in line or "error" in line or "warning" in line]
    if summary_lines:
        Logs.info("Summary of issues:")
        for line in summary_lines:
            Logs.info(line)

def lint_c_cpp_files(c_files, cpp_files, clang_format_file, log_file=None):
    """Run linting on C/C++ files"""
    all_files = c_files + cpp_files
    success = True
    
    if all_files:
        # Run clang-format
        Logs.info("Running clang-format on C/C++ files...")
        if log_file:
            with open(log_file, 'a') as f:
                f.write("===== C/C++ FORMATTING =====\n")
                
        try:
            files_needing_format = []
            for file in all_files:
                process = subprocess.run(['clang-format', 
                    '-style=file:' + clang_format_file, 
                    file], 
                    stdout=subprocess.PIPE, 
                    text=True, 
                    check=False)
                
                with open(file, 'r') as f:
                    original = f.read()
                
                if original != process.stdout:
                    files_needing_format.append(file)
                    success = False
                    
                    # Write diff to log file
                    if log_file:
                        with open(log_file, 'a') as f:
                            f.write(f"File {file} needs formatting\n")
                            try:
                                diff_process = subprocess.run(['diff', '-u', file, '-'], 
                                    input=process.stdout, 
                                    text=True,
                                    stdout=subprocess.PIPE, 
                                    check=False)
                                f.write(f"{diff_process.stdout}\n")
                            except (subprocess.SubprocessError, FileNotFoundError):
                                f.write("Could not generate diff\n")
                    else:
                        Logs.warn(f"File {file} needs formatting")
                        
            if files_needing_format:
                Logs.warn(f"{len(files_needing_format)} files need formatting - see details in {log_file}")
                
        except FileNotFoundError:
            error_msg = "clang-format not found. Install with: brew install clang-format (macOS) or apt-get install clang-format (Linux)"
            Logs.error(error_msg)
            if log_file:
                with open(log_file, 'a') as f:
                    f.write(f"ERROR: {error_msg}\n")
            success = False
        
        # Run cpplint
        Logs.info("Running cpplint on C/C++ files...")
        try:
            if log_file:
                with open(log_file, 'a') as f:
                    f.write("===== CPPLINT OUTPUT =====\n")
                result = subprocess.run(['cpplint', '--filter=-legal/copyright,-readability/braces'] + all_files, 
                                       stdout=subprocess.PIPE, 
                                       stderr=subprocess.STDOUT,
                                       text=True,
                                       check=False)
                with open(log_file, 'a') as f:
                    f.write(result.stdout)
                
                # Print summary to console
                error_count = sum(1 for line in result.stdout.splitlines() if 'error' in line.lower())
                if error_count > 0:
                    Logs.warn(f"cpplint found {error_count} issues - see details in {log_file}")
                else:
                    Logs.info("cpplint completed successfully")
            else:
                subprocess.run(['cpplint', '--filter=-legal/copyright,-readability/braces'] + all_files, check=False)
        except FileNotFoundError:
            error_msg = "cpplint not found. Install with: pip install cpplint"
            Logs.error(error_msg)
            if log_file:
                with open(log_file, 'a') as f:
                    f.write(f"ERROR: {error_msg}\n")
            success = False
            
    return success

def run_lint(ctx):
    """Run linting on the codebase"""
    Logs.info("Running linters for Python, C, and C++ code...")
    
    # Setup lint configuration files
    top = getattr(ctx, 'top', '.')
    lint_configs_dir, clang_format_file, pylintrc_file = setup_lint_configs(top)
    
    # Setup logging
    log_file = setup_logging(top)
    Logs.info(f"Lint output will be saved to: {log_file}")
    
    # Find source files
    py_files, c_files, cpp_files = find_source_files(top)
    
    # Write header to log file
    with open(log_file, 'w') as f:
        f.write(f"Lint run at {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Python files: {len(py_files)}\n")
        f.write(f"C files: {len(c_files)}\n")
        f.write(f"C++ files: {len(cpp_files)}\n\n")
    
    # Run linting on Python files
    py_success = lint_python_files(py_files, pylintrc_file, log_file)
    
    # Run linting on C/C++ files
    cpp_success = lint_c_cpp_files(c_files, cpp_files, clang_format_file, log_file)
    
    if py_success and cpp_success:
        Logs.info(f"Linting complete - all files pass. Full details in {log_file}")
    else:
        Logs.warn(f"Linting complete - some files need formatting. Full details in {log_file}")