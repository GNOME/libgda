/*
 * Copyright (C) 2009 - 2012 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
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

/*
 * This program generates a string array using *.xml.in files passed as argument
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define MAXSIZE 65536
#define MAXFILES 128

typedef struct {
	char *fname;
	int start;
} EmbFile;

EmbFile emb_files[MAXFILES];

int 
main (int argc,char** argv)
{
	char buffer[MAXSIZE];
	int maxlen = MAXSIZE-1;
	int arg_index;
	int buffer_index = 0, real_index = 0;
	int file_index;

	if (argc < 2) {
		fprintf (stderr, "Usage: %s <file1.xml.in> [...]\n", argv[0]);
		return 1;
	}
	if (argc - 1> MAXFILES) {
		fprintf (stderr, "Max number of files (%d) reached: %d files to embedd\nIncrease MAXFILES constant and re-run\n", 
			MAXFILES, argc - 1);
		return 1;
	}

	for (file_index = 0, arg_index = 1; 
	     arg_index < argc;
	     arg_index++) {

		const char *cfile = argv[arg_index];
		
		/* ignore files which do not end in ".xml.in" */
		int i, len;
		len = strlen (cfile);
		if ((len < 8) || strcmp (cfile + len - 7, ".xml.in")) {
			/* ignore that file */
			continue;
		}

		/* prepare file name to embedd: remove directory and the ".in" */
		EmbFile *efile;		
		efile = &(emb_files [file_index]);
		fprintf (stderr, "Embedding %s\n", cfile);
		for (i = len - 8; i >= 0; i--) {
			if ((cfile[i] == '/') || (cfile[i] == '\\'))
				break;
		}
		efile->fname = malloc (sizeof (char) * len - i);
		memcpy (efile->fname, cfile + i + 1, len - i);
		efile->fname[len - i - 4] = 0;
		efile->start = real_index;
		/*fprintf (stderr, "FNAME: %s\n", efile->fname);*/

		/* load file in buffer */
		FILE *file;
		long fsize;
		size_t n_read;
		
		file = fopen (cfile, "r");
		if (!file) {
			fprintf (stderr, "Can't read '%s'", cfile);
			return 1;
		}
		
		/* file size */
		fseek (file , 0 , SEEK_END);
		fsize = ftell (file);
		rewind (file);
		if (fsize + buffer_index >= maxlen) {
			fprintf (stderr, "Max buffer size reached\nIncrease MAXSIZE constant and re-run\n");
			fclose (file);
			return 1;
		}
		
		/* copy the file into the buffer: */
		n_read = fread (buffer + buffer_index, 1, fsize, file);
		if (n_read != (unsigned int) fsize) {
			fprintf (stderr, "Reading error");
			fclose (file);
			return 1;
		}

		/* alter buffer */
		int added = 0;
		int real_added = 0;
		for (i = buffer_index; (unsigned int) i < buffer_index + n_read + added; i++) {
			if (buffer_index + n_read + added + 2 >= (unsigned int) maxlen) {
				fprintf (stderr, "Max buffer size reached\nIncrease MAXSIZE constant and re-run\n");
				fclose (file);
				return 1;
			}
			if (buffer[i] == '"') {
				added ++;
				memmove (buffer + i + 1, buffer + i, buffer_index + n_read + added - i);
				buffer[i] = '\\';
				i++;
			}
			else if (buffer[i] == '\n') {
				/*
				added += 1;
				memmove (buffer + i + 1, buffer + i, buffer_index + n_read + added - i);
				buffer[i] = '\\';
				buffer[i+1] = 'n';
				i += 1;
				*/
				added += 4;
				memmove (buffer + i + 4, buffer + i, buffer_index + n_read + added - i);
				buffer[i] = '\\';
				buffer[i+1] = 'n';
				buffer[i+2] = '"';
				buffer[i+3] = '\n';
				buffer[i+4] = '"';
				i += 4;
			}
			else if ((buffer[i] == '_') && (i > buffer_index) && (buffer[i-1] == ' ')) {
				added --;
				real_added --;
				buffer[i] = ' ';
				memmove (buffer + i - 1, buffer + i, buffer_index + n_read + added - i);
				i --;
			}
		}

		real_index += n_read + real_added;
		buffer_index += n_read + added -1;
		buffer [buffer_index] = '\\';
		buffer_index ++;
		buffer [buffer_index] = '0';
		buffer_index ++;
		file_index ++;
		
		fclose (file);
	}
	buffer [buffer_index] = 0;

	/* Output */
	int i;
	
	printf ("typedef struct {\n\tgchar *fname;\n\tint start;\n} EmbFile;\n\nEmbFile emb_index[%d] = {\n",
		file_index);
	for (i = 0; i < file_index; i++) {
		EmbFile *efile;		
		efile = &(emb_files [i]);

		printf ("\t{\"%s\", %d}", efile->fname, efile->start);
		if (i < file_index - 1)
			printf (",\n");
		else
			printf ("\n");
	}
	printf ("};\n\n");
	printf ("gchar *emb_string=\"%s\";\n\n", buffer);

	printf ("static const gchar *\nemb_get_file (const gchar *fname)\n"
		"{\n\tgint i;\n\tfor (i = 0; i < %d; i++) {\n"
		"\t\tEmbFile *efile = &(emb_index[i]);\n"
		"\t\tif (!strcmp (efile->fname, fname))\n"
		"\t\t\treturn (emb_string + efile->start);\n"
		"\t}\n"
		"\treturn NULL;\n}\n\n",
		file_index);


	return 0;
}
