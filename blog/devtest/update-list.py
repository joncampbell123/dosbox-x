#!/usr/bin/python
from lxml import html
from lxml import etree

# open the template page file.
# Python will blow up with an exception here on failure.
htmt_tree = etree.parse("_page.html")
htmt_tree_root = htmt_tree.getroot()

# find the LIST_PLACEHOLDER and remove it from the tree
list_placeholder = htmt_tree_root.find("./body/LIST_PLACEHOLDER")
if not list_placeholder == None:
    list_placeholder.getparent().remove(list_placeholder)

# write the final result
htmt_tree.write("index.html")

