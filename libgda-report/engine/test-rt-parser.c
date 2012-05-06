/*
 * Copyright (C) 2011 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2011 - 2012 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <libgda/libgda.h>
#ifdef HAVE_GDKPIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#endif

#include "rt-parser.c"

typedef struct {
	gchar *in;
	gchar *exp_parsed;
	gchar *exp_docbook;
} TestCase;

TestCase test_cases[] = {
	/* 0 */
	{"aa **bb** cc",
	 "0-NONE [offset=0] |0:0-PARA [offset=0] TEXT [aa ]|0:0:0-BOLD TEXT [bb]|0:0:1-NONE [offset=0] TEXT [ cc]|",
	 "<top><para>aa <emphasis role=\"bold\">bb</emphasis> cc</para></top>"},

	{"aa **bb //ii// dd** cc",
	 "0-NONE [offset=0] |0:0-PARA [offset=0] TEXT [aa ]|0:0:0-BOLD |0:0:0:0-NONE [offset=0] TEXT [bb ]|0:0:0:1-ITALIC TEXT [ii]|0:0:0:2-NONE [offset=0] TEXT [ dd]|0:0:1-NONE [offset=0] TEXT [ cc]|",
	 "<top><para>aa <emphasis role=\"bold\">bb <emphasis>ii</emphasis> dd</emphasis> cc</para></top>"},

	{"aa \n- item **1**\n- item2\nsome text",
	 "0-NONE [offset=0] |0:0-PARA [offset=0] TEXT [aa ]|0:0:0-LIST [offset=0] |0:0:0:0-NONE [offset=0] TEXT [item ]|0:0:0:1-BOLD TEXT [1]|0:0:1-LIST [offset=0] TEXT [item2]|0:1-PARA [offset=0] TEXT [some text]|",
	 "<top><para>aa <itemizedlist><listitem><para>item <emphasis role=\"bold\">1</emphasis></para></listitem><listitem><para>item2</para></listitem></itemizedlist></para><para>some text</para></top>"},

	{"aa \n- item 1\na nice pict: [[[pictdata]]]some more text\nsome text",
	 "0-NONE [offset=0] |0:0-PARA [offset=0] TEXT [aa ]|0:0:0-LIST [offset=0] TEXT [item 1]|0:1-PARA [offset=0] TEXT [a nice pict: ]|0:1:0-PICTURE BINARY|0:1:1-NONE [offset=0] TEXT [some more text]|0:2-PARA [offset=0] TEXT [some text]|",
	 "<top><para>aa <itemizedlist><listitem><para>item 1</para></listitem></itemizedlist></para><para>a nice pict: <ulink url=\"./IMG_0000.jpg\">link</ulink>some more text</para><para>some text</para></top>"},

	{"foreword\n= chapter 1 =\nsome **bold** text\n= chapter 2 =\ntext",
	 "0-NONE [offset=0] |0:0-PARA [offset=0] TEXT [foreword]|0:1-TITLE [offset=0] TEXT [chapter 1]|0:1:0-PARA [offset=0] TEXT [some ]|0:1:0:0-BOLD TEXT [bold]|0:1:0:1-NONE [offset=0] TEXT [ text]|0:2-TITLE [offset=0] TEXT [chapter 2]|0:2:0-PARA [offset=0] TEXT [text]|",
	 "<top><para>foreword</para><sect1><title>chapter 1</title><para>some <emphasis role=\"bold\">bold</emphasis> text</para></sect1><sect1><title>chapter 2</title><para>text</para></sect1></top>"},

	/* 5 */
	{"- item 1\n - item 1.1\n - item 1.2\n- item 2\nsome text",
	 "0-NONE [offset=0] |0:0-LIST [offset=0] TEXT [item 1]|0:0:0-LIST [offset=1] TEXT [item 1.1]|0:0:1-LIST [offset=1] TEXT [item 1.2]|0:1-LIST [offset=0] TEXT [item 2]|0:2-PARA [offset=0] TEXT [some text]|",
	 "<top><itemizedlist><listitem><para>item 1<itemizedlist><listitem><para>item 1.1</para></listitem><listitem><para>item 1.2</para></listitem></itemizedlist></para></listitem><listitem><para>item 2</para></listitem></itemizedlist><para>some text</para></top>"},

	{" - item 0.1\n - item 0.2\n- item 1\n - item 1.1\n - item 1.2\n- item 3\nEnd of list",
	 "0-NONE [offset=0] |0:0-LIST [offset=0] |0:0:0-LIST [offset=1] TEXT [item 0.1]|0:0:1-LIST [offset=1] TEXT [item 0.2]|0:1-LIST [offset=0] TEXT [item 1]|0:1:0-LIST [offset=1] TEXT [item 1.1]|0:1:1-LIST [offset=1] TEXT [item 1.2]|0:2-LIST [offset=0] TEXT [item 3]|0:3-PARA [offset=0] TEXT [End of list]|",
	 "<top><itemizedlist><listitem><para><itemizedlist><listitem><para>item 0.1</para></listitem><listitem><para>item 0.2</para></listitem></itemizedlist></para></listitem><listitem><para>item 1<itemizedlist><listitem><para>item 1.1</para></listitem><listitem><para>item 1.2</para></listitem></itemizedlist></para></listitem><listitem><para>item 3</para></listitem></itemizedlist><para>End of list</para></top>"
	},

	{"Intro\n= Title 1 =\nBlah\n== Title1.1 ==\nsome text\n== Title1.2 ==\nsome other text\n= Title2 =\nsome text",
	 "0-NONE [offset=0] |0:0-PARA [offset=0] TEXT [Intro]|0:1-TITLE [offset=0] TEXT [Title 1]|0:1:0-PARA [offset=0] TEXT [Blah]|0:1:1-TITLE [offset=1] TEXT [Title1.1]|0:1:1:0-PARA [offset=0] TEXT [some text]|0:1:2-TITLE [offset=1] TEXT [Title1.2]|0:1:2:0-PARA [offset=0] TEXT [some other text]|0:2-TITLE [offset=0] TEXT [Title2]|0:2:0-PARA [offset=0] TEXT [some text]|",
	 "<top><para>Intro</para><sect1><title>Title 1</title><para>Blah</para><sect2><title>Title1.1</title><para>some text</para></sect2><sect2><title>Title1.2</title><para>some other text</para></sect2></sect1><sect1><title>Title2</title><para>some text</para></sect1></top>"},

	{"line 1\n**bold text** blah",
	 "0-NONE [offset=0] |0:0-PARA [offset=0] TEXT [line 1]|0:1-PARA [offset=0] TEXT []|0:1:0-BOLD [offset=0] TEXT [bold text]|0:1:1-NONE [offset=0] TEXT [ blah]|",
	 "<top><para>line 1</para><para><emphasis role=\"bold\">bold text</emphasis> blah</para></top>"},

	{"** bold -- still bold ** normal",
	 "0-NONE [offset=0] |0:0-BOLD [offset=0] TEXT [ bold -- still bold ]|0:1-NONE [offset=0] TEXT [ normal]|",
	 "<top><emphasis role=\"bold\"> bold -- still bold </emphasis> normal</top>"}
};

int 
main ()
{
	guint i;

	for (i = 0; i < (sizeof (test_cases) / sizeof (TestCase)); i++) {
		TestCase *test = (TestCase*) &(test_cases[i]);
		gchar *tmp;
		RtNode *tree;
		g_print ("======= TEST %d =======\n[%s]\n", i, test->in);

		/* parsing test */
		tree = rt_parse_text (test->in);
		rt_dump_tree (tree);
		tmp = rt_dump_to_string (tree);
		if (strcmp (tmp, test->exp_parsed)) {
			g_print ("EXP: %s\nGOT: %s\n", test->exp_parsed, tmp);
			return 1;
		}
		g_free (tmp);
		rt_free_node (tree);

		/* docbook converter */
		xmlNodePtr node;
		xmlKeepBlanksDefault(0);
		node = xmlNewNode (NULL, BAD_CAST "top");
		parse_rich_text_to_docbook (NULL, node, test->in);
		xmlBufferPtr buf;
		buf = xmlBufferCreate ();
		xmlNodeDump (buf, NULL, node, 1, 1);
		g_print ("XML:\n%s\n", (gchar *) xmlBufferContent (buf));
		xmlBufferFree (buf);

		buf = xmlBufferCreate ();
		xmlNodeDump (buf, NULL, node, 1, 0);
		xmlFreeNode (node);
		if (strcmp ((gchar *) xmlBufferContent (buf), test->exp_docbook)) {
			g_print ("EXP: %s\nGOT: %s\n", test->exp_docbook, (gchar *) xmlBufferContent (buf));
			return 1;
		}
		xmlBufferFree (buf);
		g_print ("Test OK\n");
	}

	g_print ("All OK!\n");
	return 0;
}
