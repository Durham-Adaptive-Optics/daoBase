# Builds the dao Node-API addon (daoShm.c) against libdao.
# Headers: ../../include (this checkout). Library: $DAOROOT/lib (an installed
# daoBase) and ../../build/src (an in-tree waf build), both also set as rpath.
{
  "targets": [
    {
      "target_name": "dao",
      "sources": ["daoShm.c"],
      "include_dirs": ["../../include"],
      "cflags": ["-std=gnu11", "-Wall", "-Wno-unused-function"],
      "conditions": [
        ["OS!='win'", {
          "libraries": [
            "-L<!(echo ${DAOROOT:-/nonexistent})/lib",
            "-L<!(echo ${DAOROOT:-/nonexistent})/lib64",
            "-L<(module_root_dir)/../../build/src",
            "-Wl,-rpath,<!(echo ${DAOROOT:-/nonexistent})/lib",
            "-Wl,-rpath,<(module_root_dir)/../../build/src",
            "-ldao"
          ]
        }]
      ]
    }
  ]
}
