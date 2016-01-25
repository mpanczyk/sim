/*	This file is part of the software similarity tester SIM.
	Written by Dick Grune, Vrije Universiteit, Amsterdam.
	$Id: sim.c,v 2.45 2015-04-29 18:18:22 dick Exp $
*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>

#include	"system.par"
#include	"settings.par"
#include	"sim.h"
#include	"options.h"
#include	"newargs.h"
#include	"token.h"
#include	"language.h"
#include	"error.h"
#include	"text.h"
#include	"runs.h"
#include	"hash.h"
#include	"compare.h"
#include	"pass1.h"
#include	"pass2.h"
#include	"pass3.h"
#include	"percentages.h"
#include	"stream.h"
#include	"lang.h"

#include	"Malloc.h"
#include	"any_int.h"

							/* VERSION */
#if	0	/* set to 1 when experimenting */
#undef	VERSION
#define	VERSION	__TIMESTAMP__
#endif
							/* PARAMETERS */
/* command-line parameters */
int Min_Run_Size = DEFAULT_MIN_RUN_SIZE;
int Page_Width = DEFAULT_PAGE_WIDTH;
int Threshold_Percentage = 1;		/* minimum percentage to show */
FILE *Output_File;
FILE *Debug_File;

/* and their string values, for language files that define their own parameters
*/
const char *token_name = "token";
const char *min_run_string;
const char *threshold_string;

const char *progname;			/* for error reporting */

static const char *page_width_string;
static const char *output_name;		/* for reporting */

static const struct option optlist[] = {
	{'r', "minimum run size", 'N', &min_run_string},
	{'w', "page width", 'N', &page_width_string},
	{'f', "function-like forms only", ' ', 0},
	{'F', "keep function identifiers in tact", ' ', 0},
	{'d', "use diff format for output", ' ', 0},
	{'T', "terse output", ' ', 0},
	{'n', "display headings only", ' ', 0},
	{'p', "use percentage format for output", ' ', 0},
	{'P', "use percentage format, main contributor only", ' ', 0},
	{'t', "threshold level of percentage to show", 'N', &threshold_string},
	{'e', "compare each file to each file separately", ' ', 0},
	{'s', "do not compare a file to itself", ' ', 0},
	{'S', "compare new files to old files only", ' ', 0},
	{'R', "recurse into subdirectories", ' ', 0},
	{'i', "read arguments (file names) from standard input", ' ', 0},
	{'o', "write output to file F", 'F', &output_name},
	{'v', "show version number and compilation date", ' ', 0},
	{'M', "show memory usage info", ' ', 0},
	{'-', "lexical scan output only", ' ', 0},
	{0, 0, 0, 0}
};

static void
allow_at_most_one_out_of(const char *opts) {
	const char *first;
	for (first = opts; *first; first++) {
		const char *second;
		for (second = first + 1; *second; second++) {
			if (is_set_option(*first) &&is_set_option(*second)) {
				char msg[256];
				sprintf(msg,
					"options -%c and -%c are incompatible",
					*first, *second
				);
				fatal(msg);
			}
		}
	}
}

							/* SERVICE ROUTINES */
int
is_new_old_separator(const char *s) {
	if (strcmp(s, "/") == 0) return 1;
	if (strcmp(s, "|") == 0) return 1;
	return 0;
}

const char *
size_t2string(size_t s) {
	return any_uint2string(s, 0);
}

							/* PROGRAM */
static void
read_and_compare_files(int argc, const char **argv, int round) {
	Read_Input_Files(argc, argv, round);
	Make_Forward_References();
	Compare_Files();
	Free_Forward_References();
}

#ifdef	ARG_TEST
static void
show_args(const char *msg, int argc, const char *argv[]) {
	fprintf(stdout, "%s: ", msg);

	int i;
	for (i = 0; i < argc; i++) {
		fprintf(stdout, "arg[%d] = %s; ", i, argv[i]);
	}
	fprintf(stdout, "\n");
}
#endif	/* ARG_TEST */

int
main(int argc, const char *argv[]) {

	/* Save program name */
	progname = argv[0];
	argv++, argc--;				/* and skip it */

	/* Set the default output and debug streams */
	Output_File = stdout;
	Debug_File = stdout;

	/* Get command line options */
	{	int nop = do_options(progname, optlist, argc, argv);
		argc -= nop, argv += nop;	/* and skip them */
	}

	/* Check options compatibility */
	allow_at_most_one_out_of("dnpPT");
	if (is_set_option('t')) {
		/* threshold means percentages */
		if (!is_set_option('p') && !is_set_option('P'))
			fatal("option -t requires -p or -P");
	}

	/* Treat the simple options */
	if (is_set_option('v')) {
		fprintf(stdout, "Version %s\n", VERSION);
		return 0;
	}

	if (is_set_option('P')) {
		set_option('p');
	}
	if (is_set_option('p')) {
		set_option('e');
		set_option('s');
	}

	/* Treat the value options */
	if (min_run_string) {
		Min_Run_Size = atoi(min_run_string);
		if (Min_Run_Size == 0)
			fatal("bad or zero run size; form is: -r N");
	}
	if (page_width_string) {
		Page_Width = atoi(page_width_string);
		if (Page_Width <= 0)
			fatal("bad or zero page width");
	}
	if (threshold_string) {
		Threshold_Percentage = atoi(threshold_string);
		if ((Threshold_Percentage > 100) || (Threshold_Percentage <= 0))
			fatal("threshold must be between 1 and 100");
	}
	if (output_name) {
		Output_File = fopen(output_name, "w");
		if (Output_File == 0) {
			char *msg = (char *)Malloc(strlen(output_name) + 100);

			sprintf(msg, "cannot open output file `%s'",
				output_name);
			fatal(msg);
			/*NOTREACHED*/
		}
	}

	/* Treat the input-determining options */
	if (is_set_option('i')) {
		/* read input file names from standard input */
		if (argc != 0)
			fatal("-i option conflicts with file arguments");
		get_new_std_input_args(&argc, &argv);
	}
	if (is_set_option('R')) {
		get_new_recursive_args(&argc, &argv);
	}
	/* (argc, argv) now represents new_file* [ / old_file*] */

	/* Here the real work starts */
	Init_Language();

	if (is_set_option('-')) {
		/* Just the lexical scan */
		while (argv[0]) {
			const char *arg = argv[0];
			if (!is_new_old_separator(arg)) {
				Print_Stream(arg);
			}
			argv++;
		}
	}
	else
	if (is_set_option('p')) {
		/* Show percentages */
		read_and_compare_files(argc, argv, 1);
		Show_Percentages();
	} else {
		/* Show runs */
		read_and_compare_files(argc, argv, 1);
		Retrieve_Runs();
		Show_Runs();
	}

	if (is_set_option('M')) {
		/* It is not trivial to plug the leaks, because data structures
		   point to each other, and have to be freed in the proper
		   order. But it is not impossible either. To do, perhaps.
		*/
		ReportMemoryLeaks(stderr);
	}

	return 0;
}
