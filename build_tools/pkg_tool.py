# build_tools/pkg_tool
from waflib.TaskGen import feature, after_method
import os

pruned_flags = ['-Wall', '-Wextra', '-pipe', '-fdiagnostics-color=auto', '-fstack-protector', '-Ofast', '-O0', '-O1', '-O2', '-O3', '-g']

def prune_c_flags(cflags):
    pruned_list=[]
    for flag in cflags:
        if flag not in pruned_flags:
            pruned_list.append(flag)
    return pruned_list

@feature('pkg_build')
@after_method('apply_core')
def add_post_build_step(self):
    #     # Access build arguments directly
    ldflags = self.to_list(getattr(self, 'ldflags', []))
    # # check if c or c++
    if('cxx' in self.env):
        c_flags = self.to_list(getattr(self, 'cxxflags', []))
    else:
        c_flags = self.to_list(getattr(self, 'cflags', []))
        
    c_flags=prune_c_flags(c_flags)
    
        # Generate pkgconfig file content
    pc_content = f"""
    prefix={self.env.PREFIX}
    exec_prefix=${{prefix}}
    libdir=${{exec_prefix}}{self.bld.env.LIBDIR.replace(self.bld.env.PREFIX, '')}
    includedir=${{prefix}}/include

    Name: {self.to_list(getattr(self,'target',[])).pop(0)}
    Description: A description of MyFeature
    Version: 1.0
    Libs: -L${{libdir}} -l{self.to_list(getattr(self,'target',[])).pop(0)} {' '.join(ldflags)}
    Cflags: -I${{includedir}} {' '.join(c_flags)}
    """
    # Write pkgconfig file
    pc_file_path = os.path.join(self.bld.bldnode.abspath(), f'{self.target}.pc')
    with open(pc_file_path, 'w') as pc_file:
        pc_file.write(pc_content)

    print(f"pkgconfig file generated: {pc_file_path}")