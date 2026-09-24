// wrapper.h
//
// Feeds daoBase's public C API to bindgen (see build.rs). Points directly
// at the header in the repo, so this builds from a fresh checkout with no
// `waf build`/`waf install` step required first.
#include "../../include/dao.h"
