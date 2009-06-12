#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <regex.h>
#include <readline/readline.h>
#include <readline/history.h>

typedef	struct	prompt_u	{
	int		doPrompt;
	char		thePrompt[ 48 ];
	char		extraPrompt[ 48 ];
} prompt_t;

typedef	enum	homer_e		{
	HOMER_none	= 0,
	HOMER_tilde,
	HOMER_macro
} homer_t;

static unsigned		nonfatal;
static char *		me;
static unsigned		debugLevel;
static char *		ifile;
static char *		ofile;
static char *		prompt;
static char *		shell;
static int		verbose;

static	prompt_t	promptInfo;

static FILE *		scriptFyle;
static char		scriptFn[ 256 ];
static char *		paths[ (BUFSIZ/2) + 1 ];
static unsigned		Npaths;
static unsigned		showAbsolutePath = 1;
static unsigned		runCommand = 1;
static regex_t		pattern;
static homer_t		homer = HOMER_tilde;
static char *		onlyCommand;

static void
vsay(
	int		e,
	char *		fmt,
	va_list		ap
)
{
	fprintf( stderr, "%s: ", me );
	vfprintf( stderr, fmt, ap );
	if( e )	{
		fprintf( stderr, "; errno=%d (%s)", e, strerror( e ) );
	}
	fprintf( stderr, ".\n" );
}

static int inline
doDebug(
	int		level
)
{
	return( level <= debugLevel );
}

static void
debug(
	int		level,
	char *		fmt,
	...
)
{
	if( doDebug( level ) )	{
		va_list	ap;

		va_start( ap, fmt );
		vsay( 0, fmt, ap );
		va_end( ap );
	}
}

static void
panic(
	int		e,
	char *		fmt,
	...
)
{
	va_list		ap;

	va_start( ap, fmt );
	vsay( e, fmt, ap );
	va_end( ap );
	exit( 1 );
	/*NOTREACHED*/
}

static void
usage(
	char *		fmt,
	...
)
{
	if( fmt )	{
		va_list	ap;
		
		fprintf( stderr, "%s: ", me );
		va_start( ap, fmt );
		vfprintf( stderr, fmt, ap );
		va_end( ap );
		fprintf( stderr, "\n" );
	}
	fprintf(
		stderr,
		"usage: %s"
		" [-D]"
		" [-a]"
		" [-c cmd]"
		" [-f script]"
		" [-h]"
		" [-o ofile]"
		" [-p prompt]"
		" [-s shell]"
		" [-v]"
		"\n",
		me
	);
	fprintf(
		stderr,
		"\t#ignore - ignore this line completely.\n"
		"\t#hide   - don't show command or results.\n"
		"\t#note   - insert editorial comment.\n"
		"\t#snip   - show and run command, elide results.\n"
		"\t#quiet  - don't show command, display results.\n"
	);
	exit( 1 );
	/*NOTREACHED*/
}

static	void
getPaths(
	void
)
{
	char *		s;
	char *		bp;

	s = strdup( getenv( "PATH" ) );
	for(
		bp = s;
		(paths[ Npaths ] = strtok( bp, ":" )) != NULL;
		bp = NULL
	)	{
		++Npaths;
	}
}

static	char *
findPath(
	char * const	token
)
{
	static	char	path[ (PATH_MAX +1) * 2 ];
	char *		retval;

	retval = token;
	if( showAbsolutePath )	{
		char * *	pp;

		for( pp = paths; *pp; ++pp )	{
			sprintf( path, "%s/%s", *pp, token );
			if( access( path, X_OK ) == 0 )	{
				retval = path;
				break;
			}
		}
	}
	return( retval );
}

static void
getProcessName(
	char *		s
)
{
	char *		bp;

	if( (bp = strrchr( s, '/' )) != NULL )	{
		s = bp + 1;
	}
	me = strdup( s );
	assert( me );
	if( (bp = strchr( me, '.' )) != NULL )	{
		*bp = '\0';
	}
}

static unsigned
processArgs(
	int		argc,
	char * *	argv
)
{
	int		c;
	unsigned	results;

	results = 0;
	while( (c = getopt( argc, argv, "DHac:f:hno:p:s:v" )) != EOF )	{
		switch( c )	{
		default:
			fprintf( stderr, "%s: no -%c yet!\n", me, c );
			/*FALLTHRU*/
		case '?':
			++results;
			break;
		case 'D':
			++debugLevel;
			break;
		case 'H':
			homer = HOMER_macro;
			break;
		case 'a':
			showAbsolutePath = 0;
			break;
		case 'c':
			onlyCommand = optarg;
			break;
		case 'f':
			ifile = optarg;
			break;
		case 'h':
			homer = HOMER_none;
			break;
		case 'n':
			runCommand = 0;
			break;
		case 'o':
			ofile = optarg;
			break;
		case 'p':
			prompt = strdup( optarg );
			break;
		case 's':
			shell = optarg;
			break;
		case 'v':
			++verbose;
			break;
		}
	}
	return( results );
}

static void
setPrompt(
	void
)
{
	uid_t const	uid = geteuid();

	prompt = uid ? "$" : "#";
}

static void
writeScript(
	char *		fmt,
	...
)
{
	char	line[ BUFSIZ ];
	va_list		ap;

	va_start( ap, fmt );
	vsnprintf( line, sizeof( line ), fmt, ap );
	if( verbose )	{
		static int	lineno = 1;
		fprintf( stderr, "line %d\t%s\n", lineno++, line );
	}
	fprintf( scriptFyle, "%s\n", line );
	va_end( ap );
}

static int
isToken(
	char const * const	buf,
	char const * const	s
)
{
	int const	len = strlen( s );
	int		results;

	results = 0;
	do	{
		if( strncasecmp( buf, s, len ) )	{
			break;
		}
		if( !isspace( buf[len] ) )	{
			break;
		}
		results = 1;
	} while( 0 );
	return( results );
}

static char *
skipWhitespace(
	char *		s
)
{
	while( *s && isspace( *s ) )	{
		++s;
	}
	return( s );
}

static unsigned
doCommand(
	char *		line
)
{
	int		hideCommand;
	int		snipResults;
	int		beQuiet;
	int		reallyRun;
	char *          bp;
	char *		verb;
	char *		rest;

	hideCommand = 0;
	snipResults = 0;
	beQuiet = 0;
	reallyRun = runCommand;
	do	{
		/* Trim whitespace				 	 */
		line = skipWhitespace( line );
		for( bp = line + strlen( line ) - 1; bp > line; )	{
			if( !isspace( *bp ) )	{
				break;
			}
			*bp-- = '\0';
		}
		debug( 2, "trimmed=|%s|", line );
                /*
                 *--------------------------------------------------------
                 * Process flags, if any.
                 *--------------------------------------------------------
                 * #ignore - ignore this line completely
		 *
                 * #hide - run command, hide command and results.
                 *
                 * #snip - display and run command, elide results.
                 *
                 * #quiet - do not announce but run anyway.
		 *
		 * #step  - identify interesting places in logged output
		 *
		 * #note  - insert parenthetical aside into output
		 *
		 * #find  - Locate and show command but do not run
                 *--------------------------------------------------------
                 */
		if( isToken( line, "#ignore" ) )	{
			goto Exit;
		} else if( isToken( line, "#hide" ) )	{
			line += 5;
			hideCommand = 1;
			beQuiet = 1;
			snipResults = 1;
		} else if( isToken( line, "#snip" ) )	{
			line += 5;
			snipResults = 1;
		} else if( isToken( line, "#quiet" ) )	{
			line += 6;
			hideCommand = 1;
		} else if( isToken( line, "#step" ) )	{
			static unsigned	step_no = 1;
			static char	new[ BUFSIZ ];
			char		label[ 32 ];

			line = skipWhitespace( line+5 );
			snprintf( label, sizeof( label ), "Step %d:", step_no );
			snprintf( new, sizeof( new ),
				"echo -e '%s%-8s %s\n'",
				(step_no == 1) ? "" : "\n",
				label,
				line
			);
			++step_no;
			line = new;
			hideCommand = 1;
		} else if( isToken( line, "#note" ) )	{
			static char	new[ BUFSIZ ];

			line = skipWhitespace( line + 5 );
			snprintf( new, sizeof( new ),
				"echo -e '>> %s <<\n'",
				line
			);
			line = new;
			hideCommand = 1;
		} else if( isToken( line, "#find" ) )	{
			line += 5;
			hideCommand = 0;
			reallyRun = 0;
		} else if( line[0] == '#' )	{
			/* Any other splat is not ours			 */
			break;
		}
		line = skipWhitespace( line );
	} while( isspace( line[0] ) || (line[0] == '#') );
	verb = strtok( line, " \t\n" );
	rest = verb + strlen( verb ) + 1;
	verb = findPath( verb );
        if( !hideCommand )      {
		writeScript( "cat <<-'EOF'" );
		writeScript( "%s %s %s", prompt, verb, rest );
		writeScript( "EOF" );
        }
	if( reallyRun )	{
		writeScript(
			"%s %s %s",
			verb,
			rest,
			(snipResults ? ">/dev/null 2>&1" : "")
		);
		if( snipResults && !beQuiet )	{
			writeScript( "echo '<output snipped>'" );
		}
	}
Exit:
	return( 0 );
}

static void
getShell(
	void
)
{
	shell = getenv( "SHELL" );
	if( !shell )	{
		shell = "/bin/sh";
	}
}

static void
removeScriptFile(
	void
)
{
	if( scriptFn[0] )	{
		if( debugLevel > 1 )	{
			fprintf( 
				stderr, 
				"%s: leaving '%s' for your inspection.\n",
				me,
				scriptFn
			);
		} else	{
			unlink( scriptFn );
		}
	}
}

static void
openScriptFile(
	void
)
{
	int		fd;
	char *		tmpdir;

	tmpdir = getenv( "TMP" );
	if( !tmpdir )	{
		tmpdir = getenv( "TMPDIR" );
		if( !tmpdir )	{
			tmpdir = "/tmp";
		}
	}
	snprintf( 
		scriptFn, 
		sizeof( scriptFn ), 
		"%s/scr.%d.tmp.XXXXXX",
		tmpdir,
		getpid()
	);
	debug( 1, "template=%s", scriptFn );
	fd = mkstemp( scriptFn );
	if( fd < 0 )	{
		panic( errno, "cannot create temp file '%s'", scriptFn );
	}
	debug( 1, "scriptFn=%s", scriptFn );
	scriptFyle = fdopen( fd, "r+t" );
	if( !scriptFyle )	{
		panic( errno, "fdopen failed" );
	}
	if( atexit( removeScriptFile ) )	{
		panic( errno, "could not register removeScriptFile" );
	}
}

static void
initPrompt(
	prompt_t *	tp
)
{
	tp->doPrompt = isatty( 0 );
	if( tp->doPrompt )	{
		snprintf( tp->thePrompt, sizeof( tp->thePrompt ), "%s> ", me );
		snprintf( tp->extraPrompt, sizeof( tp->extraPrompt ), "%s(cont)> ", me );
	}
}

static char *
getCommand(
	prompt_t *	tp
)
{
	int		len;
	char *		theLine;
	char *		part;

	len = 0;
	theLine = NULL;
	do	{
		int	n;

		/* Get a command segment				 */
		if( tp->doPrompt )	{
			/* Input from a terminal			 */
			part = readline( 
				len ? tp->extraPrompt : tp->thePrompt
			);
		} else	{
			char *	buf;

			buf = part = malloc( BUFSIZ );
			if( !buf )	{
				panic( errno, "out of memory" );
				/*NOTREACHED*/
			}
			part = fgets( buf, BUFSIZ, stdin );
			if( !part )	{
				/* EOF, don't leak buffer		 */
				free( buf );
			} else	{
				/* Drop line terminator			 */
				part[ strlen( part ) - 1 ] = '\0';
			}
		}
		if( !part )	{
			break;
		}
		debug( 2, "read part |%s|", part );
		n = strlen( part );
		/* Append part to end of the whole line			 */
		theLine = realloc( theLine, len + n + 1 );
		if( !theLine )	{
			free( part );
			break;
		}
		strncpy( theLine + len, part, n );
		len += n;
		theLine[ len ] = '\0';
		free( part );
		debug( 2, "input so far |%s|", theLine );
		/* Break out if line is not continued			 */
		if( theLine[ len - 1 ] != '\\' )	{
			break;
		}
		theLine[ len - 1 ] = ' ';
	} while( theLine );
	return( theLine );
}

static void
freeCommand(
	char *		cmd
)
{
	free( cmd );
}

static	void
setupHomer(
	void
)
{
	char * const	home = getenv( "HOME" );
	char		buf[ BUFSIZ + 1 ];

	/* Divide into three parts: leadin, ${HOME}, trailer		 */
	sprintf( buf, "^\\(.*\\)\\(%s\\)\\(.*\\)$", home );
	/* Compile into the pattern buffer				 */
	if( regcomp( &pattern, buf, RE_SYNTAX_SED ) )	{
		perror( buf );
		exit( 1 );
	}
	debug( 1, "homer-pattern=|%s|", buf );
}

static	void
printPart(
	char * const		s,
	regmatch_t * const	rm
)
{
	printf( "%.*s", rm->rm_eo - rm->rm_so, s + rm->rm_so );
}

int
main(
	int		argc,
	char * *	argv
)
{
	char		line[ BUFSIZ ];
	FILE *		cmd;

	getProcessName( argv[0] );
	getPaths();
	getShell();
	setPrompt();
	nonfatal += processArgs( argc, argv );
	if( nonfatal )	{
		usage( "illegal argument(s)" );
		/*NOTREACHED*/
	}
	setupHomer();
	if( ofile )	{
		unlink( ofile );
		if( freopen( ofile, "wt", stdout ) != stdout )	{
			panic( errno, "cannot redirect output to '%s'",
				ofile );
		}
	}
	openScriptFile();
	if( onlyCommand )	{
		nonfatal += doCommand( onlyCommand );
	} else if( optind < argc )	{
		/* Take command from the command line		 */
		int	i;
		char *	bp;
		char *	lbp;

		bp = line;
		lbp = bp + sizeof( line );
		for( i = optind; i < argc; ++i )	{
			bp += snprintf(
				bp,
				lbp - bp,
				"%s%s",
				((i == optind) ? "" : " "),
			   	argv[ i ]
			);
		}
		nonfatal += doCommand( line );
	} else	{
		/* Read commands from stdin, redirecting if needed	 */
		if( ifile && 
		(freopen( ifile, "rt", stdin ) != stdin ) )	{
			panic( errno, "cannot redirect input to '%s'",
					ifile );
			/*NOTREACHED*/
		}
		initPrompt( &promptInfo );
		while( !feof( stdin ) )	{
			char *		line;

			/* Read input line			 */
			line = getCommand( &promptInfo );
			if( !line )	{
				if( promptInfo.doPrompt )	{
					fprintf( stderr, " EOF\n" );
				}
				break;
			}
			nonfatal += doCommand( line );
			freeCommand( line );
		}
	}
	fclose( scriptFyle );
	snprintf(
		line,
		sizeof( line ),
		"%s %s 2>&1",
		shell,
		scriptFn
	);
	cmd = popen( line, "r" );
	if( !cmd )	{
		panic( errno, "could not open pipe '%s'", line );
	}
	while( fgets( line, sizeof( line ), cmd ) )	{
#		define		PATTERNS	4
		regmatch_t	parts[PATTERNS];

		if( homer == HOMER_none )	{
			fputs( line, stdout );
		}  else	{
			if( !regexec( &pattern, line, PATTERNS, parts, 0 ) ) {
				regmatch_t * const	leadin = parts + 1;
				regmatch_t * const	trailer = parts + 3;

				printPart( line, leadin );
				printf(
					(homer == HOMER_macro) ? "${HOME}" :
						"~"
				);
				printPart( line, trailer );
				printf( "\n" );
			} else	{
				fputs( line, stdout );
			}
		}
	}
	pclose( cmd );
	return( nonfatal ? 1 : 0 );
}
