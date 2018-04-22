#!/usr/bin/python
from lxml import html
from lxml import etree
import stat
import os
import re

# open the template page file.
# Python will blow up with an exception here on failure.
htmt_tree = etree.parse("_page.html")
htmt_tree_root = htmt_tree.getroot()

# list and enumerate blog entries.
# each one is a directory of the form YYYY-MM-DD-HHMMSS
blogents = [ ]
dirreg = re.compile('^\d+', re.IGNORECASE)
for dirname in os.listdir("."):
    try:
        st = os.lstat(dirname)
        if stat.S_ISDIR(st.st_mode):
            if dirreg.match(dirname):
                try:
                    st2 = os.lstat(dirname+"/_page.html")
                    if stat.S_ISREG(st2.st_mode):
                        blogents.append(dirname)
                except:
                    True
    except:
        True

# load each blog
blogtree = { }
for ent in blogents:
    print "Loading ent..."
    broot = etree.parse(ent+"/_page.html")
    blogtree[ent] = broot

# get the title, if possible
blogtitles = { }
for ent in blogtree:
    tree = blogtree[ent]
    root = tree.getroot()
    title = root.find("./head/title")
    if not title == None:
        if not title.text == None and not title.text == "":
            blogtitles[ent] = title.text

# find the LIST_PLACEHOLDER and remove it from the tree
list_placeholder = htmt_tree_root.find("./body/LIST_PLACEHOLDER")
if not list_placeholder == None:
    list_placeholder_index = list_placeholder.getparent().index(list_placeholder)
    list_placeholder_parent = list_placeholder.getparent()
    list_placeholder_parent.remove(list_placeholder)
    #
    list_tbl = etree.SubElement(list_placeholder_parent, "table", attrib={"width":"100%"})
    #
    list_placeholder_parent.insert(list_placeholder_index,list_tbl)

# write the final result
htmt_tree.write("index.html")

