#!/usr/bin/python
from lxml import html
from lxml import etree
import stat
import sys
import os
import re

operate_on = None
if len(sys.argv) > 1:
    operate_on = sys.argv[1]
    #
    st = os.lstat(operate_on)
    if not stat.S_ISDIR(st.st_mode):
        print "Must be a blog entry"
        sys.exit(1)

# parameters---------

# maximum blog posts to show contents on main page
max_posts_on_page = 10

# end parameters-----------

# ent to date
def ent2date(x):
    # YYYY-MM-DD-HHMMSS
    # YYYY-MM-DD-HHMM
    y = x.split('-')
    r = ''
    #
    year = y[0]
    mon = y[1]
    day = y[2]
    hm = y[3]
    hour = hm[0:2]
    minute = hm[2:4]
    second = hm[4:6]
    #
    r = year + '/' + mon + '/' + day
    if not hour == None and not hour == "":
        r += ' ' + hour
        if not minute == None and not minute == "":
            r += ':' + minute
            if not second == None and not second == "":
                r += ':' + second
    #
    return r

# open the template page file.
# Python will blow up with an exception here on failure.
blogents = [ ]
if operate_on == None:
    htmt_tree = etree.parse("_page.html")

    #----------------------------------
    # list and enumerate blog entries.
    # each one is a directory of the form YYYY-MM-DD-HHMMSS
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
    #----------------------------------
else:
    htmt_tree = etree.parse(operate_on+"/_page.html")
    blogents.append(operate_on)
#
htmt_tree_root = htmt_tree.getroot()

# sort into descending order
blogents.sort(reverse=True)

# load each blog
blogtree = { }
for ent in blogents:
    print "Loading " + ent + "..."
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

# free all but first. the array is sorted in descending order.
if operate_on == None:
    count = 0
    for ent in blogents:
        if ent in blogtree and count > max_posts_on_page:
            tree = blogtree[ent]
            blogtree.pop(ent, None)
        count = count + 1

# find the LIST_PLACEHOLDER and remove it from the tree
if operate_on == None:
    list_placeholder = htmt_tree_root.find("./body/LIST_PLACEHOLDER")
    if not list_placeholder == None:
        list_placeholder_index = list_placeholder.getparent().index(list_placeholder)
        list_placeholder_parent = list_placeholder.getparent()
        list_placeholder_parent.remove(list_placeholder)
        #
        list_tbl = etree.SubElement(list_placeholder_parent, "table", attrib={"width":"100%"})
        #
        for ent in blogents:
            if ent in blogtree:
                tree = blogtree[ent] # I want Python to blow up with an exception if this is ever None or invalid
            else:
                tree = None
            #
            title = blogtitles[ent]
            if title == None or title == "":
                title = "(no title)"
            #
            row = etree.SubElement(list_tbl, "tr")
            list_tbl.append(row)
            #
            rowtitle = etree.SubElement(row, "div")
            rowtitle.set("style","font-size: 1.4em; padding-bottom: 1em;");
            #
            href = ent + "/index.html";
            rowtitle_p1 = etree.SubElement(rowtitle, "a", attrib={"href":href})
            rowtitle_p1.text = title
            rowtitle.append(rowtitle_p1)
            #
            rowtitle_p2 = etree.SubElement(rowtitle, "span")
            rowtitle_p2.text = u" \u2014 " + ent2date(ent)
            rowtitle_p2.set("style","font-size: 0.85em;");
            rowtitle.append(rowtitle_p2)
            #
            rowtitle_p3 = etree.SubElement(rowtitle, "br")
            rowtitle.append(rowtitle_p3)
            #
            row.append(rowtitle)
            #
            if not tree == None:
                content = tree.find('./body')
                if not content == None:
                    content.tag = 'div'
                    content.set("style","position: relative; top: 0px; left: 0px;");
                    row.append(content)
                    #
                    rowpad1 = etree.SubElement(row, "br")
                    row.append(rowpad1)
        #
        list_placeholder_parent.insert(list_placeholder_index,list_tbl)
# one page
else:
    page_body = htmt_tree_root.find("./body")
    page_body.getparent().remove(page_body)
    page_body.tag = 'div'
    #
    newbody = etree.SubElement(htmt_tree_root, "body")
    #
    titletext = "(no title)"
    title = htmt_tree_root.find("./head/title")
    if not title == None:
        titletext = title.text
        title.text = title.text + ' ' + ent2date(operate_on)
    #
    rowtitle = etree.SubElement(newbody, "h1")
    rowtitle.set("style","font-size: 1.4em; padding-bottom: 1em;");
    rowtitle.text = titletext
    #
    rowtitle_p2 = etree.SubElement(rowtitle, "span")
    rowtitle_p2.text = u" \u2014 " + ent2date(operate_on)
    rowtitle_p2.set("style","font-size: 0.85em;");
    rowtitle.append(rowtitle_p2)
    #
    newbody.append(rowtitle)
    #
    newbody.append(page_body)
    #
    htmt_tree_root.append(newbody)

# write the final result
if operate_on == None:
    htmt_tree.write("index.html", encoding='utf-8')
else:
    htmt_tree.write(operate_on+"/index.html", encoding='utf-8')

