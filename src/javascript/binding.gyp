# Builds the dao Node-API addon (daoShm.c) against libdao.
# Headers: ../../include (this checkout). Library: ../../build/src (an in-tree
# waf build) first, else $DAOROOT/lib (an installed daoBase); both also set as
# rpath, $DAOROOT/lib first. waf install runs this with DAOROOT=its prefix.
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
            "-L<(module_root_dir)/../../build/src",
            "-L<!(echo ${DAOROOT:-/nonexistent})/lib",
            "-L<!(echo ${DAOROOT:-/nonexistent})/lib64",
            "-Wl,-rpath,<!(echo ${DAOROOT:-/nonexistent})/lib",
            "-Wl,-rpath,<(module_root_dir)/../../build/src",
            "-ldao"
          ]
        }]
      ]
    }
  ]
}
