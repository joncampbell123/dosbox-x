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
    list_placeholder_index = list_placeholder.getparent().index(list_placeholder)
    list_placeholder_parent = list_placeholder.getparent()
    list_placeholder_parent.remove(list_placeholder)
    #
    list_tbl = etree.SubElement(list_placeholder_parent, "table", attrib={"width":"100%"})
    #
    list_placeholder_parent.insert(list_placeholder_index,list_tbl)

# write the final result
htmt_tree.write("index.html")

