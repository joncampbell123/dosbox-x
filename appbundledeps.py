#!/usr/bin/env python3

import re
import os
import sys
import subprocess

# EXEs to scan for dependencies
exepaths = [ ]
deps = { }

# look for argv
sit = iter(sys.argv)
next(sit) # toss argv[0]

class DepInfo:
    slname = None
    modpath = None
    exepath = None
    rpath = None
    loaderpath = None # @loader_path
    dependencies = None
    def __init__(self,modpath=None,exepath=None,slname=None):
        self.modpath = str(modpath)
        self.slname = slname
        self.loaderpath = None
        self.rpath = [ ]
        self.exepath = exepath;
        if not modpath == None:
            self.loaderpath = os.path.basename(modpath)
        self.dependencies = [ ]
    def __str__(self):
        return "[modpath="+str(self.modpath)+",loaderpath="+str(self.loaderpath)+",exepath="+str(self.exepath)+"]"

def help():
    print("appbundledeps.py --exe <exe>")

def GetDepList(exe,modpath=None,exepath=None):
    rl = [ ]
    rpath = [ ]
    #
    p = subprocess.Popen(["otool","-l",exe],stdout=subprocess.PIPE,encoding="utf8")
    ldcmd = None
    for lin in p.stdout:
        lin = lin.strip().split(' ')
        if len(lin) == 0:
            continue
        if lin[0] == "cmd" and len(lin) > 1:
            ldcmd = lin[1]
        if lin[0] == "path" and len(lin) > 1 and not lin[1] == "" and ldcmd == "LC_RPATH":
            rpath.append(lin[1])
    #
    p = subprocess.Popen(["otool","-L",exe],stdout=subprocess.PIPE,encoding="utf8")
    for lin in p.stdout:
        if lin == None or lin == "":
            continue
        # we're looking for anything where the first char is tab
        if not lin[0] == '\t':
            continue
        #
        lin = lin.strip().split(' ')
        if len(lin) == 0:
            continue
        deppath = lin[0].split('/')
        #
        if deppath[0] == "@rpath":
            if len(rpath) > 0:
                deppath = rpath[0].split('/') + deppath[1:]
        #
        if deppath[0] == "@loader_path":
            deppath = os.path.dirname(modpath).split('/') + deppath[1:]
        # dosbox-x refers to the name of the dylib symlink not the raw name, store that name!
        slname = deppath[-1]
        # NTS: Realpath is needed because Brew uses symlinks and .. rel path resolution will fail trying to access /opt/opt/...
        if len(deppath[0]) > 0 and deppath[0][0] == "@":
            deppath = '/'.join(deppath)
        else:
            deppath = os.path.realpath('/'.join(deppath))
        if deppath == None:
            raise Exception("Unable to resolve")
        #
        di = DepInfo(modpath=deppath,exepath=exepath,slname=slname)
        di.rpath = rpath
        rl.append(di)
    #
    p.terminate()
    return rl

while True:
    try:
        n = next(sit)
        if n == '--exe':
            exepaths.append(os.path.realpath(next(sit)))
        elif n == '-h' or n == '--help':
            help()
            sys.exit(1)
        else:
            print("Unknown switch "+n)
            sys.exit(1)
    except StopIteration:
        break

if len(exepaths) == 0:
    print("Must specify EXE")
    sys.exit(1)

for exe in exepaths:
    dl = GetDepList(exe,modpath=exe,exepath=exe)
    for dep in dl:
        if not dep.modpath in deps:
            dep.dependencies = GetDepList(dep.modpath,modpath=dep.modpath,exepath=exe)
            deps[dep.modpath] = dep

while True:
    newdeps = 0
    tdeps = deps.copy()
    for deppath in tdeps:
        depobj = deps[deppath]
        for dep in depobj.dependencies:
            if not dep.modpath in deps:
                newdeps += 1
                dep.dependencies = GetDepList(dep.modpath,modpath=dep.modpath,exepath=exe)
                deps[dep.modpath] = dep
    #
    if newdeps == 0:
        break

for deppath in deps:
    # do not list /usr/lib or /System libraries, only /opt (Brew) dependencies
    # TODO: Make an option to list them if wanted
    if re.match(r"^/opt/",deppath):
        depobj = deps[deppath]
        print(str(deppath)+"\t"+str(depobj.slname))

