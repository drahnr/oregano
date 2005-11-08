/* Author: Carlos Lacasta. */

/* TODO: We need to ask Carlos Lacasta to add an implicit GPLv2 notice here. */

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <glib.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

struct Tree_ {
	xmlNodePtr symbols;
	xmlNodePtr parts;
	FILE *pf;
};

typedef struct Tree_ Tree;

struct Property_ {
	char *name;
	char *value;
};
typedef struct Property_ Property;

struct Symbol_ {
   int  size;
   long offs;
   GSList *connections;
   GSList *objects;
   GSList *properties;
   GSList *labels;
   char *desc;
   char *model;
   char *refdes;
   char *template;
   char *name;
   char *ako;
   int	width,height,x0,y0;
};
typedef struct Symbol_ Symbol;

static char buf[4096],word[1024],nam[1024],sdum[1024];
static char c,*ptr,*qtr,*line;
static char sName[4][80];
GHashTable *symbTable;

void
list_clear(GSList **slist, int flg) {
   GSList *list;
   for (list = *slist;list;list=list->next) {
	   if ( flg ) {
		   Property *prop = (Property *)list->data;
		   free( prop->name );
		   free( prop->value);
	   }
	   free( list->data );
   }
   g_slist_free(  *slist );
   *slist=NULL;
}

void
symbol_clear(Symbol *symb) {
   free (symb->desc);
   free (symb->model);
   free (symb->refdes);
   free (symb->template);
   free (symb->name );
   free (symb->ako);

   symb->desc	  = NULL;
   symb->model	  = NULL;
   symb->refdes	  = NULL;
   symb->template = NULL;
   symb->name	  = NULL;
   symb->ako	  = NULL;

   if ( symb->properties  ) list_clear(&symb->properties,1);
   if ( symb->connections ) list_clear(&symb->connections,0);
   if ( symb->properties )	list_clear(&symb->properties,0);
   if ( symb->objects )		list_clear(&symb->objects,0);
   if ( symb->labels )		list_clear(&symb->labels,0);
}

void
GetSymbol(Symbol *symb,FILE *pf,long offs) {
   int ix,iy,ix1,iy1,is,iok;
   char cc;
   if ( symb->name &&  strstr(symb->name,"27") ) {
	   printf("Aqui estamos");
   }
   symbol_clear(symb);

   /*  Go to the symbol position in the file   */
   fseek(pf,offs,SEEK_SET);
   *sName[1] = 0;
   fgets(buf,1024,pf);
   iok = sscanf(buf,"%*s %s %s %s %s",sName[0],sName[1],sName[2],sName[3]);
   symb->name = strdup(sName[0]);
   if ( iok==3 ) {
	   if ( strstr(sName[1],"ako")==sName[1] )
		   symb->ako = strdup(sName[2]);
   }
   else if ( iok==4 ) {
	   if ( strstr(sName[2],"ako")==sName[2] )
		   symb->ako = strdup(sName[3]);
   }

   for ( c = getc(pf); c!='*' && c!=EOF; c=getc(pf)) {
	   switch (c) {
	   case 'd': /* description */
		   fgets(buf,1024,pf);
		   buf[strlen(buf)-1]=0;
		   symb->desc = strdup(buf);
		   break;
	   case '@':
		   fgets(buf,1024,pf);
		   if ( strstr(buf,"type") ) {
		   /*  do nothing */
		   }
		   /*-------------------------------------------------------------------
			* Connections
			*------------------------------------------------------------------*/
		   else if ( strstr(buf,"pins") ) {
			   for ( c=getc(pf);c!=EOF && c!='@' && c!='*';c=getc(pf)) {
				   fgets(buf,1024,pf);
				   if ( c=='p' ) {
					   int xl,yl;
					   char dir;
					   iok = sscanf(buf,"%*d %d %d %*s %*d %s %*s %d %d %c",
									&xl,&yl,sdum,&ix,&iy,&dir);
					   if (iok==6 ) {
						   sprintf(word,"(%d %d)",ix,iy);
						   symb->connections
							   = g_slist_prepend(symb->connections,strdup(word));

						   /* Do not add that many labels
							*  sprintf(word,"%s 0 (%d %d)",sdum,xl,yl);
							*  symb->labels =
							*  g_slist_prepend(symb->labels,strdup(word));
							*/
						   /*  Add the drawings of the connections */
						   switch (dir) {
						   case 'u':
							   sprintf(word,"line 0 (%d %d)(%d %d)",ix-10,iy,ix,iy);
							   break;
						   case 'h':
							   sprintf(word,"line 0 (%d %d)(%d %d)",ix+10,iy,ix,iy);
							   break;
						   case 'd':
							   sprintf(word,"line 0 (%d %d)(%d %d)",ix,iy+10,ix,iy);
							   break;
						   case 'v':
							   sprintf(word,"line 0 (%d %d)(%d %d)",ix,iy-10,ix,iy);
							   break;
						   }
						   symb->objects
							   = g_slist_prepend(symb->objects,strdup(word));
					   }
				   }
			   }
			   if ( c=='@' ) { ungetc(c,pf); }
		   }

		   /*--------------------------------------------------------------
			* Graphics
			*--------------------------------------------------------------*/
		   else if ( strstr(buf,"graphics") ) {
			   int width,height,x0,y0,nch;
			   sscanf(buf,"%*s %*d %d %d %d %d",&width,&height,&x0,&y0);
			   symb->width=width;
			   symb->height=height;
			   symb->x0 = x0;
			   symb->y0 = y0;
			   x0=y0=0;
			   for ( c=getc(pf);c!=EOF && c!='@' && c!='*';c=getc(pf)) {
				   switch (c) {
				   case 'a': /*	 arc  */
					   fgets(buf,1024,pf);
					   qtr = buf;
					   ptr = word;
					   iok = sscanf(qtr,"%*d %d %d %n",&ix,&iy,&nch);
					   if ( iok==2 ) {
						   qtr += nch;
						   ptr += sprintf(ptr,"line 1 (%d %d)",ix-x0,iy-y0);
						   while ( sscanf(qtr,"%d %d %n",&ix,&iy,&nch)==2 ) {
							   ptr += sprintf(ptr,"(%d %d)",ix-x0,iy-y0);
							   qtr += nch;
						   }
						   symb->objects =
							   g_slist_prepend(symb->objects,strdup(word));
					   }
					   break;
				   case 'c': /* circle */
					   fgets(buf,1024,pf);
					   iok = sscanf(buf,"%*d %d %d %d",&ix,&iy,&ix1);
					   if (iok==3) {
						   sprintf(word,"arc (%d %d)(%d %d)",
								   ix-ix1-x0,iy-ix1-y0,ix+ix1-x0,iy+ix1-y0);
						   symb->objects =
							   g_slist_prepend(symb->objects,strdup(word));
					   }
					   break;
				   case 'r': /* rectangle  */
					   fgets(buf,1024,pf);
					   iok = sscanf(buf,"%*d %d %d %d %d",&ix,&iy,&ix1,&iy1);
					   if ( iok==4 ) {
						   sprintf(word,"line 0 (%d %d)(%d %d)(%d %d)(%d %d)(%d %d)",
								   ix-x0,iy-y0,
								   ix-x0,iy1-y0,
								   ix1-x0,iy1-y0,
								   ix1-x0,iy-y0,
								   ix-x0,iy-y0
								   );
						   symb->objects =
							   g_slist_prepend(symb->objects,strdup(word));
					   }

					   break;
				   case 'v': /* line  */
					   is = 0;
					   for (cc=getc(pf); cc!=EOF && cc!=';';cc=getc(pf),is++) {
						   if ( cc=='\n' ) cc=' ';
						   buf[is] = cc;
					   }
					   if ( cc==';' ) fgets(word,1024,pf);
					   buf[is]=0;
					   qtr = buf;
					   ptr = word;
					   iok = sscanf(qtr,"%*d %d %d %d %d %n",
									&ix,&iy,&ix1,&iy1,&nch);
					   if ( iok==4 ) {
						   qtr += nch;
						   ptr += sprintf(ptr,"line 0 (%d %d)(%d %d)",
							ix-x0,iy-y0,ix1-x0,iy1-y0);

						   while ( sscanf(qtr,"%d %d %n",&ix,&iy,&nch)==2 ) {
							   ptr += sprintf(ptr,"(%d %d)",ix-x0,iy-y0);
							   qtr += nch;
						   }

						   symb->objects =
							   g_slist_prepend(symb->objects,strdup(word));
					   }

					   break;
				   case 's': /* string */
					   fgets(buf,1024,pf);
					   iok = sscanf(buf,"%*d %d %d %*s %*d %n",&ix,&iy,&nch);
					   if ( iok==2 ) {
						   sprintf(word,"text (%d %d)",ix-symb->x0,iy-symb->y0);
						   strcat(word,buf+nch);
						   symb->objects =
							   g_slist_prepend(symb->objects,strdup(word));
					   }
					   break;
				   default:
					   fgets(buf,1024,pf);
					   break;
				   }
			   }
			   if ( c=='@' ) { ungetc(c,pf); }
		   }
		   /*----------------------------------------------------------------
			* Attributes
			*--------------------------------------------------------------*/
		   else if ( strstr(buf,"attributes") ) {
			   char *q,*str,att[10];
			   int showme;
			   int	isprop=0;
			   for ( c=getc(pf);c!=EOF && c!='@' && c!='*';c=getc(pf) ) {
				   fgets(buf,1024,pf);
				   iok=sscanf(buf," %*d %s %d %*d %d %d %*s %*d %n",
							  att,&showme,&ix,&iy,&is);
				   str = buf+is;
				   for (ptr=str; *ptr ;ptr++)  {
					   if ( *ptr =='\\' )
						   if ( ptr[1]=='n' || ptr[1]=='t' || ptr[1]=='r' ) {
							   ptr++;
							   continue;
						   }
					   *ptr = toupper(*ptr);
				   }
				   q = strchr(str,'=');
				   if ( q ) {
					   *q++=0;
					   if (*q==0 || *q=='\n') q=0;
					   if ( q ) {
						   char *pq;
						   pq = q + strlen(q);
						   for ( ;pq>q;pq--)
							   if ( *pq == '\r' || *pq == '\n' )
								   *pq=0;
					   }
				   }
				   isprop = (int)strchr(att,'u');
				   if ( showme==9 || showme==13 ) {
					   sprintf(word,"%s 1 (%d %d)",str,ix,iy);
					   symb->labels =
						   g_slist_prepend(symb->labels,strdup(word));
				   }

				   if ( isprop ) {
					   Property *prop;
					   prop = (Property *)malloc(sizeof(Property));
					   prop->name = strdup(str);
					   prop->value= ( q ? strdup(q) : 0 );
					   symb->properties =
						   g_slist_prepend(symb->properties,prop);
				   }
				   else if ( (line=strstr(str,"REFDES"))!=NULL && line==str) {
					   if ( q ) {
						   line = strchr(q,'?');
						   if (line ) *line=0;
						   symb->refdes = strdup(q);
					   }
				   }
				   else if ( (line=strstr(str,"MODEL"))!=NULL && line==str) {
					   if ( q )symb->model = strdup( q );
				   }
				   else if ( (line=strstr(str,"TEMPLATE"))!=NULL &&
							 line==str) {
					   if ( q ) {
						   for (ptr=q;*ptr;ptr++) {
							   if ( *ptr=='^' ) *ptr='_';
							   if ( *ptr=='\n') *ptr=' ';
						   }
						   symb->template = strdup( q );
					   }
				   }
			   }
			   if ( c=='@' ) { ungetc(c,pf); }
		   }
		   break;
	   }
	   if ( c=='*') ungetc(c,pf);
   }
}

void
AddAko( Symbol *symb,FILE *pf,long offs) {
	Symbol *ako;
	if ( symb->ako ) {
		ako = (Symbol *)g_hash_table_lookup(symbTable,symb->ako);
		if ( ako==0 ) return;
		if ( !ako->name ) GetSymbol(ako,pf,ako->offs);
		if ( !ako->model || !ako->refdes || !ako->template || !ako->connections)
			AddAko(ako,pf,ako->offs);
		if ( !symb->model && ako->model	 )
			symb->model	   = strdup( ako->model	   );
		if ( !symb->refdes	&& ako->refdes )
			symb->refdes   = strdup( ako->refdes   );
		if ( !symb->template && ako->template )
			symb->template = strdup( ako->template );
		/*
		  if ( !symb->connections && ako->connections ) {
		  GSList *list;
		  for (list = ako->connections;list;list=list->next) {
		  if ( list->data )
		  symb->connections = g_slist_prepend( symb->connections,
		  strdup((char *)list->data)
		  );
		  else {
		  printf("null data in connection list: %s\n",ako->name);
		  }
		  }
		  }
		*/
		return;
	}
	else  return;
}

void tbl_buildTree(char *key, Symbol *symb, Tree *tree) {
	int is,editable;
	char *q;

	xmlNodePtr symbol,part,prop,dmy;
	/*	Get the symbol from the file*/
	if ( !strcmp(key,"27") ) {
		printf("Aqui estamos\n");
	}
	GetSymbol(symb, tree->pf,symb->offs);
	printf("Processing symbol %s\n",(char *)key);


	/*	Check if it is a kind of something */
	if ( symb->ako ) AddAko(symb,tree->pf,symb->offs);

	if ( symb->connections || symb->objects ) {
		GSList *list;
		symbol = xmlNewChild(tree->symbols, NULL, BAD_CAST "ogo:symbol",NULL);
		xmlNewChild(symbol, NULL, BAD_CAST "ogo:name", BAD_CAST symb->name);

		if ( symb->objects ) {
			dmy=xmlNewChild(symbol,NULL,BAD_CAST "ogo:objects",NULL);
			for (list = symb->objects;list;list=list->next) {
				sscanf(list->data,"%s %n",word,&is);
				sprintf(buf,"ogo:%s",word);
				xmlNewChild(dmy,NULL,BAD_CAST buf,BAD_CAST list->data+is);
			}
		}

		if ( symb->connections ) {
			dmy=xmlNewChild(symbol,NULL,BAD_CAST "ogo:connections",NULL);

			for (list = symb->connections;list;list=list->next)
				xmlNewChild(dmy,NULL,BAD_CAST "ogo:connection",list->data);
		}
	}

	part   = xmlNewChild(tree->parts, NULL, BAD_CAST "ogo:part", NULL);
	xmlNewChild(part, NULL, BAD_CAST "ogo:name", BAD_CAST symb->name);
	xmlNewChild(part, NULL, BAD_CAST "ogo:symbol", BAD_CAST (symb->ako ? symb->ako : symb->name));
	xmlNewChild(part, NULL, BAD_CAST "ogo:description", BAD_CAST symb->desc);


	prop =	xmlNewChild(part, NULL, BAD_CAST "ogo:properties", NULL);
	if ( symb->properties ) {
		GSList *list;

		for (list = symb->properties;list;list=list->next) {
			Property *pp = (Property *)list->data;
			dmy = xmlNewChild(prop,NULL,BAD_CAST "ogo:property",NULL);
			if (pp-> name) xmlNewChild(dmy,NULL,BAD_CAST "ogo:name",BAD_CAST pp->name);
			if (pp->value) xmlNewChild(dmy,NULL,BAD_CAST "ogo:value",BAD_CAST pp->value);
		}
	}
	if (symb->template) {
		dmy = xmlNewChild(prop,NULL,BAD_CAST "ogo:property",NULL);
		xmlNewChild(dmy,NULL,BAD_CAST "ogo:name",BAD_CAST "Template");
		xmlNewChild(dmy,NULL,BAD_CAST "ogo:value",BAD_CAST symb->template);
	}
	if (symb->refdes) {
		dmy = xmlNewChild(prop,NULL,BAD_CAST "ogo:property",NULL);
		xmlNewChild(dmy,NULL,BAD_CAST "ogo:name",BAD_CAST "RefDes");
		xmlNewChild(dmy,NULL,BAD_CAST "ogo:value",BAD_CAST symb->refdes);
	}
	if (symb->model) {
		dmy = xmlNewChild(prop,NULL,BAD_CAST "ogo:property",NULL);
		xmlNewChild(dmy,NULL,BAD_CAST "ogo:name",BAD_CAST "Model");
		xmlNewChild(dmy,NULL,BAD_CAST "ogo:value",BAD_CAST symb->model);
	}

	if ( symb->labels ) {
		GSList *list;
		prop =	xmlNewChild(part,NULL,BAD_CAST "ogo:labels",NULL);
		for (list = symb->labels;list;list=list->next) {
			sscanf(list->data,"%s %d %n",word,&editable,&is);
			dmy = xmlNewChild(prop,NULL,BAD_CAST "ogo:label",NULL);
			xmlNewChild(dmy,NULL, BAD_CAST "ogo:name", BAD_CAST word);
			xmlNewChild(dmy,NULL, BAD_CAST "ogo:position", BAD_CAST list->data+is);
			if ( editable ) {
				for (q=word;*q;q++) *q = tolower(*q);
				sprintf(buf,"@%s",word);
				xmlNewChild(dmy,NULL, BAD_CAST "ogo:text", BAD_CAST buf);
				xmlNewChild(dmy,NULL, BAD_CAST "ogo:modify", BAD_CAST "yes");
			}
			else {
				xmlNewChild(dmy,NULL, BAD_CAST "ogo:text", BAD_CAST word);
				xmlNewChild(dmy,NULL, BAD_CAST "ogo:modify", BAD_CAST "no");
			}
		}
	}
}

void
tbl_destroy(gpointer *key,Symbol *symb, gpointer *ud) {
	symbol_clear(symb);
	free ( symb );
}

int
main (int argc, char **argv)
{
	long off,off0;
	xmlDocPtr  doc;
	xmlNodePtr name, description, author, symbols, parts;
	xmlNodePtr symbol, part, dmy, conn;
	char *p,*q,oFile[256],*iFile=argv[1];
	Symbol *symb;
	Tree   tree;

	if (argc < 3) {
		g_print ("Usage: %s [infile] [outfile]\n", g_basename (argv[0]));
		return 1;
	}

	if ( iFile==0 ) return 1;

	strcpy(sdum,iFile);
	p = strrchr(sdum,'/');
	if (p ) p++;
	else p=sdum;
	q = strrchr(p,'.');
	if ( q ) *q=0;
	sprintf(oFile,"%s.oreglib",p);
	printf("Output in %s\n",p);

	doc = xmlNewDoc(BAD_CAST "1.0");
	doc->children= xmlNewDocNode(doc, NULL, BAD_CAST "ogo:library", NULL);
	name = xmlNewChild(doc->children, NULL, BAD_CAST "ogo:name", BAD_CAST p);
	xmlNewChild(doc->children, NULL, BAD_CAST "ogo:description",BAD_CAST "PSpice eval library");
	xmlNewChild(doc->children, NULL, BAD_CAST "ogo:version",BAD_CAST "1.0");
	tree.symbols = xmlNewChild(doc->children, NULL, BAD_CAST "ogo:symbols",NULL);
	tree.parts	 = xmlNewChild(doc->children, NULL, BAD_CAST "ogo:parts",NULL);

	/*	Get the symbols */
	sprintf(buf,"%s.slb",iFile);
	tree.pf =  fopen(buf ,"r" );
	if ( !tree.pf ) {
		printf("Cannot open symbol lib.: %s\n",buf);
		return 1;
	}


	/*-----------------------------------------------------------------------
	 * Make a list with all the symbols in the file
	 *-----------------------------------------------------------------------*/
	symbTable = g_hash_table_new(g_str_hash,g_str_equal);
	rewind(tree.pf);
	off = off0 = ftell(tree.pf);
	while ( fgets(buf,1024,tree.pf) ) {
	  if ( strstr(buf,"*symbol")==buf ) {
		  sscanf(buf,"%*s %s %s %s",sName[0],sName[1],sName[2]);
		  symb = (Symbol *)malloc(sizeof(Symbol));
		  memset(symb,0,sizeof(Symbol));
		  symb->offs=off;
		  symb->size=off-off0;
		  g_hash_table_insert(symbTable,strdup(sName[0]),symb);
		  off0 = off;
	  }
	  off = ftell(tree.pf);
	}
	rewind(tree.pf);
	printf("Size of Hash table %d\n",g_hash_table_size(symbTable));


	/*-----------------------------------------------------------------------
	 * Loop on the symbols and build the xml tree
	 *-----------------------------------------------------------------------*/
	g_hash_table_foreach(symbTable,(GHFunc)tbl_buildTree,(gpointer) &tree);

	xmlSetDocCompressMode (doc,9);
	xmlSaveFile(oFile,doc);
	g_hash_table_foreach(symbTable,(GHFunc)tbl_destroy,0);
	g_hash_table_destroy (symbTable);
}
