#!/usr/bin/python3

import sys
import re

MAPPING = {
    'core_read.cpp': 'core_io.cpp',
    'core_write.cpp': 'core_io.cpp',
}

def module_name(path):
    if path in MAPPING:
        path = MAPPING[path]
    if path.endswith(".h"):
        return path[:-2]
    if path.endswith(".c"):
        return path[:-2]
    if path.endswith(".cpp"):
        return path[:-4]
    return None

files = dict()
deps = dict()

RE = re.compile("^#include <(.*)>")

for arg in sys.argv[1:]:
    #print("* Scanning of %s" % arg)
    module = module_name(arg)
    if module is None:
        print("Ignoring file %s (does not constitute module)\n" % arg)
    else:
        files[arg] = module
        deps[module] = set()

for arg in files.keys():
    #print("* Reading of %s" % arg)
    module = files[arg]
    with open(arg, 'r') as f:
        for line in f:
            match = RE.match(line)
            if match:
                include = match.group(1)
                included_module = module_name(include)
                if included_module is not None and included_module in deps and included_module != module:
                    #print("DEP %s on %s\n" % (module, included_module))
                    deps[module].add(included_module)

#print("DEPS %r" % deps)

for module in deps.keys():
    #print("* Closure of %s" % module)
    closure = dict()
    for dep in deps[module]:
        #print("** Init %s" % dep)
        closure[dep] = []
    while True:
        old_size = len(closure)
        old_closure_keys = list(closure.keys())
        for src in old_closure_keys:
            for dep in deps[src]:
                if dep not in closure:
                    closure[dep] = closure[src] + [src]
        if len(closure) == old_size:
            break
    #print("Dependencies of %s: %r" % (module, closure))
    if module in closure:
        print("Circular dependency: %s through %s" % (module, ", ".join(closure[module])))
