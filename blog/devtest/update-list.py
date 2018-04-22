#!/usr/bin/python
from lxml import html
from lxml import etree

# open the template page file.
# Python will blow up with an exception here on failure.
htmt_tree = etree.parse("_page.html")

# write the final result
indexhtml = open("index.html","wb")
htmt_tree.write(indexhtml)
indexhtml.close();

