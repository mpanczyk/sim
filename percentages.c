/*	This file is part of the software similarity tester SIM.
	Written by Dick Grune, Vrije Universiteit, Amsterdam.
	$Id: percentages.c,v 1.18 2015-01-18 15:33:07 dick Exp $
*/

#include	<stdio.h>

#include	"debug.par"
#include	"sim.h"
#include	"text.h"
#include	"runs.h"
#include	"options.h"
#include	"Malloc.h"
#include	"error.h"
#include	"percentages.h"

struct match {
	struct match *ma_next;
	const char *ma_fname0;
	const char *ma_fname1;
	size_t ma_size;			/* # tokens of file 0 found in file 1 */
	size_t ma_size0;		/* # tokens in file 0 */
};

static struct match *match_start = 0;	/* to be allocated by new() */

static void
do_add_to_precentages(struct chunk ch0, struct chunk ch1, size_t size);

void
add_to_percentages(struct run *r) {
	/* percentages are only meaningful between different files */
	if (r->rn_chunk0.ch_text == r->rn_chunk1.ch_text) return;

	do_add_to_precentages(r->rn_chunk0, r->rn_chunk1, r->rn_size);
	do_add_to_precentages(r->rn_chunk1, r->rn_chunk0, r->rn_size);
}

static void
do_add_to_precentages(struct chunk ch0, struct chunk ch1, size_t size) {
	struct match **match_hook = &match_start;

	/* look up the (text0, text1) combination in the match list */
	while (*match_hook) {
		struct match *m = *match_hook;

		if (	m->ma_fname0 == ch0.ch_text->tx_fname
		&&	m->ma_fname1 == ch1.ch_text->tx_fname
		) {
			/* found it; now update it */
			m->ma_size += size;
			return;
		}
		match_hook = &m->ma_next;
	}

	{	/* it's not there; make a new entry */
		struct match *m = *match_hook = new(struct match);
		struct text *text0 = ch0.ch_text;
		struct text *text1 = ch1.ch_text;

		m->ma_next = 0;
		m->ma_fname0 = text0->tx_fname;
		m->ma_fname1 = text1->tx_fname;
		m->ma_size = size;
		m->ma_size0 = text0->tx_limit - text0->tx_start;
	}
}

							/* PRINTING */
/* We want the sorting order
      all contributors of the file with the highest percentage
      all contributors of the file with the next lower percentage
      etc.
   but this order cannot be specified by a single SORT_BEFORE().
   So we sort for percentage, and then reorder during printing.
*/

/* instantiate sort_match_list(struct match **listhook) */
static float
match_percentage(struct match *m) {
	return (((float)m->ma_size)/((float)m->ma_size0));
}
#define	SORT_STRUCT		match
#define	SORT_NAME		sort_match_list
#define	SORT_BEFORE(p1,p2)	(match_percentage(p1) > match_percentage(p2))
#define	SORT_NEXT		ma_next
#include	"sortlist.bdy"

static void
print_perc_info(struct match *m) {
	int mp = (int)(match_percentage(m)*100.0);

	if (mp > 100) {
		/* this may result from overlapping matches */
		mp = 100;
	}
	if (mp >= Threshold_Percentage) {
		fprintf(Output_File,
			"%s consists for %d %% of %s material\n",
			m->ma_fname0, mp, m->ma_fname1
		);
	}
}

static void
print_and_remove_perc_info_for_top_file(struct match **m_hook) {
	struct match *m = *m_hook;
	const char *fname = m->ma_fname0;

	print_perc_info(m);		/* always print main contributor */
	*m_hook = m->ma_next;
	Free(m);

	while ((m = *m_hook)) {
		if (m->ma_fname0 == fname) {
			/* print subsequent contributors only if not
			   suppressed by -P
			*/
			if (!is_set_option('P')) {
				print_perc_info(m);
			}
			/* remove the struct */
			*m_hook = m->ma_next;
			Free(m);
		} else {
			/* skip the struct */
			m_hook = &m->ma_next;
			continue;
		}
	}
}

static void
print_percentages(void) {
	/* destroys the match list while printing */
	while (match_start) {
		print_and_remove_perc_info_for_top_file(&match_start);
	}
}

#ifdef	DB_PERC
static void
print_match_list(const char *msg) {
	fprintf(Debug_File, "\n\n**** DB_PERCENTAGES %s ****\n", msg);
	struct match *ma;

	for (ma = match_start; ma; ma = ma->ma_next) {
		fprintf(Debug_File, "%s < %s, %d/%d=%3.0f%%\n",
			ma->ma_fname0, ma->ma_fname1,
			ma->ma_size, ma->ma_size0,
			match_percentage(ma)*100
		);
	}
	fprintf(Debug_File, "\n");
}
#endif	/* DB_PERC */

void
Show_Percentages(void) {
#ifdef	DB_PERC
	print_match_list("before sort");
#endif	/* DB_PERC */
	sort_match_list(&match_start);
#ifdef	DB_PERC
	print_match_list("after sort");
#endif	/* DB_PERC */
	print_percentages();
}
