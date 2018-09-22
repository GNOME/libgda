/*
** Compile and run this standalone program in order to generate code that
** implements a function that will translate alphabetic identifiers into
** parser token codes.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* if TEST_RESERVED_WORDS is defined, then the test_keywords() function is created
 * which displays errors for any keyword
 */
#ifndef TEST_RESERVED_WORDS
#ifdef GDA_DEBUG
#define TEST_RESERVED_WORDS
#endif
#endif

/*
** A header comment placed at the beginning of generated code.
*/
static const char zHdr[] = 
	"/* file contains automatically generated code, DO NOT MODIFY *\n"
	" *\n"
	" * The code in this file implements a function that determines whether\n"
	" * or not a given identifier is really an SQL keyword.  The same thing\n"
	" * might be implemented more directly using a hand-written hash table.\n"
	" * But by using this automatically generated code, the size of the code\n"
	" * is substantially reduced.  This is important for embedded applications\n"
	" * on platforms with limited memory.\n"
	" *\n"
	" * This code has been copied from SQLite's mkkeywordhash.c file and modified.\n"
	" * to read the SQL keywords from a file instead of static ones\n"
	" */\n";

/*
** All the keywords of the SQL language are stored in a hash
** table composed of instances of the following structure.
*/
typedef struct Keyword Keyword;
struct Keyword {
	char *zName;         /* The keyword name */
	int id;              /* Unique ID for this record */
	int hash;            /* Hash on the keyword */
	int offset;          /* Offset to start of name string */
	int len;             /* Length of this keyword, not counting final \000 */
	int prefix;          /* Number of characters in prefix */
	int longestSuffix;   /* Longest suffix that is a prefix on another word */
	int iNext;           /* Index in aKeywordTable[] of next with same hash */
	int substrId;        /* Id to another keyword this keyword is embedded in */
	int substrOffset;    /* Offset into substrId for start of this keyword */
	char zOrigName[40];  /* Original keyword name before processing */
};

Keyword *aKeywordTable = NULL;
int nKeyword = 0;
char *prefix = NULL;

/* An array to map all upper-case characters into their corresponding
** lower-case character. 
*/
const unsigned char UpperToLower[] = {
	0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,
	18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
	36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
	54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,
	104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,
	122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,
	108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,
	126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
	144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,
	162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,
	180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,
	198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,
	216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,
	234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,
	252,253,254,255
};

/*
** Comparision function for two Keyword records
*/
static int keywordCompare1(const void *a, const void *b){
	const Keyword *pA = (Keyword*)a;
	const Keyword *pB = (Keyword*)b;
	int n = pA->len - pB->len;
	if( n==0 ){
		n = strcmp(pA->zName, pB->zName);
	}
	return n;
}
static int keywordCompare2(const void *a, const void *b){
	const Keyword *pA = (Keyword*)a;
	const Keyword *pB = (Keyword*)b;
	int n = pB->longestSuffix - pA->longestSuffix;
	if( n==0 ){
		n = strcmp(pA->zName, pB->zName);
	}
	return n;
}
static int keywordCompare3(const void *a, const void *b){
	const Keyword *pA = (Keyword*)a;
	const Keyword *pB = (Keyword*)b;
	int n = pA->offset - pB->offset;
	return n;
}

/*
** Return a KeywordTable entry with the given id
*/
static Keyword *findById(int id){
	int i;
	for(i=0; i<nKeyword; i++){
		if( aKeywordTable[i].id==id ) break;
	}
	return &aKeywordTable[i];
}

/* @len is the number of chars, NOT including the \0 at the end */
static void
add_keywork (const char *keyword, int len)
{
#define MAXKEYWORDS 1000
	const char *ptr;

	if (len == 0)
		return;
	for (ptr = keyword; *ptr && (ptr - keyword) < len; ptr++) {
		if (((*ptr < 'A') || (*ptr > 'Z')) &&
		    ((*ptr < 'a') || (*ptr > 'z')) &&
		    ((*ptr < '0') || (*ptr > '9')) &&
		    (*ptr != '_')) {
			/* ignore this keyword */
			return;
		}
	}

	aKeywordTable [nKeyword].zName = malloc (sizeof (char) * (len + 1));
	memcpy (aKeywordTable [nKeyword].zName, keyword, sizeof (char) * len);
	aKeywordTable [nKeyword].zName [len] = 0;
	
	printf (" * KEYWORD: '%s'\n", aKeywordTable [nKeyword].zName);
	
	nKeyword ++;
	if (nKeyword > MAXKEYWORDS) {
		fprintf (stderr, "Maximum number of SQL keywords exceeded, "
			 "change the MAXKEYWORDS constant and recompile\n");
		exit (1);
	}
}

static void
parse_input (const char *filename)
{
#define BUFSIZE 500
	FILE *stream;
        char buffer[BUFSIZE+1];
        int read;
        char *end;

	stream = fopen (filename, "r");
	if (!stream)
		return;

	printf ("\n/* Parsed keywords\n");

	aKeywordTable = malloc (sizeof (Keyword) * MAXKEYWORDS);
	memset (aKeywordTable, 0, sizeof (Keyword) * MAXKEYWORDS);
        read = fread (buffer, 1, BUFSIZE, stream);
        end = buffer + read;
	*end = 0;
        while (read > 0) {
		char *ptr;

                /* read all complete lines in buffer */
                while (end > buffer) {
                        char *hold = NULL;
                        for (ptr = buffer; (ptr < end) && *ptr && (*ptr != '\n'); ptr++);
                        if (ptr == end)
                                break;
                        if (*ptr)
                                hold = ptr+1;
                        *ptr = 0;
                        
                        /* treat the line */
			if (*buffer && *buffer != '#') {
				char *tmp1, *tmp2;
				printf (" *\n * From line: %s\n", buffer);
				for (tmp1 = tmp2 = buffer; *tmp2; tmp2++) {
					if (((*tmp2 < 'A') || (*tmp2 > 'Z')) &&
					    ((*tmp2 < 'a') || (*tmp2 > 'z')) &&
					    ((*tmp2 < '0') || (*tmp2 > '9')) &&
					    (*tmp2 != '_')) {
						/* keyword found */
						add_keywork (tmp1, tmp2 - tmp1);
						
						tmp1 = tmp2 + 1;
					}
				}
				if ((tmp1 != tmp2) && *tmp1)
					add_keywork (tmp1, tmp2 - tmp1);
			}
                                
                        if (hold) {
                                int l = end - hold;
                                end -= hold - buffer;
                                memmove (buffer, hold, l);
                        }
                        else
                                break;
                }

		read = fread (end, 1, BUFSIZE - (end - buffer), stream);
                end += read;
	}

	printf (" */\n");

#ifdef TEST_RESERVED_WORDS
	int i;
	printf ("static char *%skeywords[] = {\n", prefix ? prefix : "");
	for (i = 0; i < nKeyword; i++)
		printf ("\t\"%s\",\n", aKeywordTable [i].zName);
	printf ("};\n");
#endif

	fclose (stream);
}

/*
** This routine does the work.  The generated code is printed on standard
** output.
*/
int
main (int argc, char **argv)
{
	int i, j, k, h;
	int bestSize, bestCount;
	int count;
	int nChar;
	int totalLen = 0;
	int aHash[5000];  /* 2000 is much bigger than nKeyword */
	char zText[10000];

	if ((argc < 2)  || (argc > 3)) {
		printf ("Usage: %s <file with SQL keywords> [<prefix>]\n", argv[0]);
		return 1;
	}
	if (argc == 3)
		prefix = argv[2];

	printf("%s", zHdr);
	parse_input (argv[1]);
	if (!aKeywordTable) {
		printf ("Error parsing file '%s', aborting\n", argv[1]);
		return 1;
	}

	/* Fill in the lengths of strings and hashes for all entries. */
	for(i=0; i<nKeyword; i++){
		Keyword *p = &aKeywordTable[i];
		p->len = strlen(p->zName);
		assert( (unsigned int) p->len < sizeof(p->zOrigName) );
		strcpy(p->zOrigName, p->zName);
		totalLen += p->len;
		p->hash = (UpperToLower[(int)p->zName[0]]*4) ^
			(UpperToLower[(int)p->zName[p->len-1]] *3) ^ p->len;
		p->id = i+1;
	}

	/* Sort the table from shortest to longest keyword */
	qsort(aKeywordTable, nKeyword, sizeof(aKeywordTable[0]), keywordCompare1);

	/* Look for short keywords embedded in longer keywords */
	for(i=nKeyword-2; i>=0; i--){
		Keyword *p = &aKeywordTable[i];
		for(j=nKeyword-1; j>i && p->substrId==0; j--){
			Keyword *pOther = &aKeywordTable[j];
			if( pOther->substrId ) continue;
			if( pOther->len<=p->len ) continue;
			for(k=0; k<=pOther->len-p->len; k++){
				if( memcmp(p->zName, &pOther->zName[k], p->len)==0 ){
					p->substrId = pOther->id;
					p->substrOffset = k;
					break;
				}
			}
		}
	}

	/* Compute the longestSuffix value for every word */
	for(i=0; i<nKeyword; i++){
		Keyword *p = &aKeywordTable[i];
		if( p->substrId ) continue;
		for(j=0; j<nKeyword; j++){
			Keyword *pOther;
			if( j==i ) continue;
			pOther = &aKeywordTable[j];
			if( pOther->substrId ) continue;
			for(k=p->longestSuffix+1; k<p->len && k<pOther->len; k++){
				if( memcmp(&p->zName[p->len-k], pOther->zName, k)==0 ){
					p->longestSuffix = k;
				}
			}
		}
	}

	/* Sort the table into reverse order by length */
	qsort(aKeywordTable, nKeyword, sizeof(aKeywordTable[0]), keywordCompare2);

	/* Fill in the offset for all entries */
	nChar = 0;
	for(i=0; i<nKeyword; i++){
		Keyword *p = &aKeywordTable[i];
		if( p->offset>0 || p->substrId ) continue;
		p->offset = nChar;
		nChar += p->len;
		for(k=p->len-1; k>=1; k--){
			for(j=i+1; j<nKeyword; j++){
				Keyword *pOther = &aKeywordTable[j];
				if( pOther->offset>0 || pOther->substrId ) continue;
				if( pOther->len<=k ) continue;
				if( memcmp(&p->zName[p->len-k], pOther->zName, k)==0 ){
					p = pOther;
					p->offset = nChar - k;
					nChar = p->offset + p->len;
					p->zName += k;
					p->len -= k;
					p->prefix = k;
					j = i;
					k = p->len;
				}
			}
		}
	}
	for(i=0; i<nKeyword; i++){
		Keyword *p = &aKeywordTable[i];
		if( p->substrId ){
			p->offset = findById(p->substrId)->offset + p->substrOffset;
		}
	}

	/* Sort the table by offset */
	qsort(aKeywordTable, nKeyword, sizeof(aKeywordTable[0]), keywordCompare3);

	/* Figure out how big to make the hash table in order to minimize the
	** number of collisions */
	bestSize = nKeyword;
	bestCount = nKeyword*nKeyword;
	for(i=nKeyword/2; i<=2*nKeyword; i++){
		for(j=0; j<i; j++) aHash[j] = 0;
		for(j=0; j<nKeyword; j++){
			h = aKeywordTable[j].hash % i;
			aHash[h] *= 2;
			aHash[h]++;
		}
		for(j=count=0; j<i; j++) count += aHash[j];
		if( count<bestCount ){
			bestCount = count;
			bestSize = i;
		}
	}

	/* Compute the hash */
	for(i=0; i<bestSize; i++) aHash[i] = 0;
	for(i=0; i<nKeyword; i++){
		h = aKeywordTable[i].hash % bestSize;
		aKeywordTable[i].iNext = aHash[h];
		aHash[h] = i+1;
	}

	/* Begin generating code */
	printf("/* Hash score: %d */\n", bestCount);
	printf("static int %skeywordCode(const char *z, int n){\n", prefix ? prefix : "");
	printf("  /* zText[] encodes %d bytes of keywords in %d bytes */\n",
	       totalLen + nKeyword, nChar+1 );
	for(i=j=k=0; i<nKeyword; i++){
		Keyword *p = &aKeywordTable[i];
		if( p->substrId ) continue;
		memcpy(&zText[k], p->zName, p->len);
		k += p->len;
		if( j+p->len>70 ){
			printf("%*s */\n", 74-j, "");
			j = 0;
		}
		if( j==0 ){
			printf("  /*   ");
			j = 8;
		}
		printf("%s", p->zName);
		j += p->len;
	}
	if( j>0 ){
		printf("%*s */\n", 74-j, "");
	}
	printf("  static const char %szText[%d] = {\n", prefix ? prefix : "", nChar);
	zText[nChar] = 0;
	for(i=j=0; i<k; i++){
		if( j==0 ){
			printf("    ");
		}
		if( zText[i]==0 ){
			printf("0");
		}else{
			printf("'%c',", zText[i]);
		}
		j += 4;
		if( j>68 ){
			printf("\n");
			j = 0;
		}
	}
	if( j>0 ) printf("\n");
	printf("  };\n");

	printf("  static const unsigned int %saHash[%d] = {\n", prefix ? prefix : "", bestSize);
	for(i=j=0; i<bestSize; i++){
		if( j==0 ) printf("    ");
		printf(" %d,", aHash[i]);
		j++;
		if( j>12 ){
			printf("\n");
			j = 0;
		}
	}
	printf("%s  };\n", j==0 ? "" : "\n");    

	printf("  static const unsigned int %saNext[%d] = {\n", prefix ? prefix : "", nKeyword);
	for(i=j=0; i<nKeyword; i++){
		if( j==0 ) printf("    ");
		printf(" %3d,", aKeywordTable[i].iNext);
		j++;
		if( j>12 ){
			printf("\n");
			j = 0;
		}
	}
	printf("%s  };\n", j==0 ? "" : "\n");    

	printf("  static const unsigned char %saLen[%d] = {\n", prefix ? prefix : "", nKeyword);
	for(i=j=0; i<nKeyword; i++){
		if( j==0 ) printf("    ");
		printf(" %3d,", aKeywordTable[i].len+aKeywordTable[i].prefix);
		j++;
		if( j>12 ){
			printf("\n");
			j = 0;
		}
	}
	printf("%s  };\n", j==0 ? "" : "\n");    

	printf("  static const unsigned short int %saOffset[%d] = {\n", prefix ? prefix : "", nKeyword);
	for(i=j=0; i<nKeyword; i++){
		if( j==0 ) printf("    ");
		printf(" %3d,", aKeywordTable[i].offset);
		j++;
		if( j>12 ){
			printf("\n");
			j = 0;
		}
	}
	printf("%s  };\n", j==0 ? "" : "\n");


	printf("  int h, i;\n");
	printf("  if( n<2 ) return 0;\n");
	printf("  h = ((charMap(z[0])*4) ^\n"
	       "      (charMap(z[n-1])*3) ^\n"
	       "      n) %% %d;\n", bestSize);
	printf("  for(i=((int)%saHash[h])-1; i>=0; i=((int)%saNext[i])-1){\n",
	       prefix ? prefix : "", prefix ? prefix : "");
	printf("    if( %saLen[i]==n &&"
	       " casecmp(&%szText[%saOffset[i]],z,n)==0 ){\n",
	       prefix ? prefix : "",
	       prefix ? prefix : "",
	       prefix ? prefix : "");
	printf("      return 1;\n");
	printf("    }\n");
	printf("  }\n");
	printf("  return 0;\n");
	printf("}\n");
	printf("\nstatic gboolean\n%sis_keyword (const char *z)\n{\n", prefix ? prefix : "");
	printf("\treturn %skeywordCode(z, strlen (z));\n", prefix ? prefix : "");
	printf("}\n\n");

#ifdef TEST_RESERVED_WORDS
	printf("\nstatic void\n%stest_keywords (void)\n{\n", prefix ? prefix : "");
	printf("\tint i;\n");
	printf("\tfor (i = 0; i < %d; i++) {\n", nKeyword);
	printf("\t\tif (! %sis_keyword (%skeywords[i]))\n", prefix ? prefix : "", prefix ? prefix : "");
	printf("\t\t\tg_print (\"KEYWORK %%s ignored!\\n\", %skeywords[i]);\n", prefix ? prefix : "");
	printf("\t}\n");
	printf("}\n\n");
#endif


	return 0;
}
