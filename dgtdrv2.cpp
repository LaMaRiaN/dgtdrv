//===================================================================
//
// DGT Digital Chess Board driver engine using dgtnix
//
// $Id: dgtdrv2.cpp,v 1.22 2009/05/06 16:13:10 arwagner Exp $
//
// Last change: <Wed, 2009/05/06 18:01:23 arwagner ingata>
//
// Author	  : Alexander Wagner
// Language   : C/C++
//
//===================================================================
//
// Copyright (C) 2006-2007 Alexander Wagner
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
// General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
//-------------------------------------------------------------------

using namespace std;
#include <iostream>
#include <cstdlib>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

extern int errno;
#include "dgtnix.h"

#define REVISIONTAG "$Revision: 1.22 $"

// xboard or uci-mode? 
// default to uci, as initialisation messages then get the additional
// "string" tag for info which is required for uci but does not
// disturb xboard.
#define XBOARD   0
#define UCI      1
#define CRAFTY   2
#define NONE     99

// Command message strings
#define MSG_MOVENOW             "!move now!\n"
#define MSG_STARTSETUP          "!enter setup mode!\n"
#define MSG_ENDSETUP            "!end setup mode!\n"
#define MSG_WHITETOMOVE         "!white to move!\n"
#define MSG_BLACKTOMOVE         "!black to move!\n"

#define MSG_NEWGAME             "!new game!\n"
#define MSG_ENDGAME10           "!end game 1-0!\n1-0\n"
#define MSG_ENDGAME01           "!end game 0-1!\n0-1\n"
#define MSG_ENDGAME11           "!end game 1/2-1/2!\n1/2-1/2\n"

// informative strings
#define MSG_SENDWHITE           "# sending white moves only\n"
#define MSG_SENDBLACK           "# sending black moves only\n"
#define MSG_SENDBOTH            "# sending both moves\n"
#define MSG_CLOCKRIGHT          "# Clock to Whites RIGHT\n"
#define MSG_CLOCKLEFT           "# Clock to Whites LEFT\n"

#define MSG_MOVETIMING          "# Move sensitivity %.2i:%.2i secs\n"
#define MSG_DEBUGLEVEL          "# Debug level set to %s\n"
#define MSG_ANNOUNCE_ON         "# Move announcement: ON\n"
#define MSG_ANNOUNCE_OFF        "# Move announcement: OFF\n"

// DGT Clock Handling

//      Definitions by DGT not in dgtnix
#define MESSAGE_BIT         0x80
#define DGT_BWTIME          0x0d
#define DGT_MSG_BWTIME      (MESSAGE_BIT|DGT_BWTIME)
#define DGT_SIZE_BWTIME     10
//      Message for dgtdrv
#define MSG_WHITETIME "# Time White: %i\n"
#define MSG_BLACKTIME "# Time Black: %i\n"
#define MSG_NOCLOCK   "# No Clock detected\n"

// move handling
#define MSG_MOVINGPIECE         "moving piece: %c"
#define MSG_TAKES               "%c takes %c"
#define MSG_CURRENTBOARD        " |====== Current DGT board ======|\n"

// xboard/uci string prefixes
#define MSG_INFOPREFIX          "info "
#define MSG_STRPREFIX           "string "
#define MSG_ERRORPREFIX         "ERROR : "
#define MSG_MOVESTRING          "\nmove %s\n"
#define MSG_OPPONENTMOVE        "# Opponent move: %s\n"
#define MSG_WRONGOPPONENTMOVE   "# Wrong move performed: %s instead of %s!\n"
// Error messages
#define MSG_UNABLETOOPEN        "Unable to open %s for writing\n"
#define MSG_UNABLEPORT          "Unable to open port: %s\n\n"
#define MSG_NODGTBOARD          "Board is not responding!\n"
#define MSG_UNRECOGDGTRESPONSE  "Unrecognised response form dgtInit.\n\n"

// Strings for the logfile
#define MSG_SESSIONSTART        "----------[ Session Start ]----------\n"
#define MSG_SESSIONEND          "\n\n----------[ Session Ended ]----------\n"

// Initialisation answers
#define MSG_INITIALISING        "Initialising...\n"
#define MSG_CHESSBOARDFOUND     "Chessboard found and initialised.\n"
#define MSG_DGTSERIALNO         "DGT Serial No. : %s\n"
#define MSG_DGTVERSION          "DGT Version    : %.6s\n"
#define MSG_DGTBUSADDRESS       "BUS Address    : %s\n"
#define MSG_DGTNIXVERSION       "dgtnix Version : %.6s\n"
#define MSG_DGTCLOCKINFO        "DGT Chess Clock: %s\n"
#define MSG_DGTPORT             "DGT connection : %s\n"

#define MSG_XBOARDMODE          "Engine mode    : xboard\n"
#define MSG_UCIMODE             "Engine mode    : UCI\n"
#define MSG_CRAFTYMODE          "Engine mode    : crafty\n"

// Several unspecific text messages
#define MSG_FOUND               "FOUND"
#define MSG_NONE                "NONE"
#define MSG_OFF                 "OFF"
#define MSG_ON                  "ON"
#define MSG_FULL                "FULL"

// shorten Trademark message to 58 chars. This corresponds to
// the usual TM for a real board but drops the linebreaks. Lines not
// starting with the "info string" prefix might confuse a GUI.
#define MSG_DGTTMSTRING         "%.58s\n"

// for yet unimplemented functions:
#define MSG_NOTIMPLEMENTED      "Not implemented: %s\n"

// if a command is sent by the GUI/User prepend it in log:
#define MSG_READSTDIN           "%s %i bytes: %s"
#define LOGINPUT                "<-"

// command ot announce moves
#define SPEAK         "speak"

#define SENDBOTH      0
#define SENDWHITE     1
#define SENDBLACK     2
#define SETUPMODE     3

#define PRGAUTHOR     "A. Wagner"

// Assume we are talking to crafty by default. Though crafty uses an
// XBoard like interface it doesn't send neither the "xboard" nor the
// "uci" startup sequence, so as long as those do not appear we are
// talking to crafty itself, and for a board dump the 
//     "command setboard"
// prefix is required for compatibility. Otherwise change this prefix
// to a normal "info"-line valid in XBoard or UCI respectively.
// 
int MODE = CRAFTY;

//  Mode of the DGT Board, i.e. which moves to send, 
//  or if to send any at all.
int DGTMODE = SENDBOTH;
// store mode temprarily to be able to restore it
int DGTINITIALMODE = DGTMODE;

// program name, may be set by the user to his own name
char   *PRGNAME         = "DGT Driver Engine $Revision: 1.22 $";

// Write all messages to an additional log file
char   LogFileName[255] = "/tmp/dgtdrv.log";
// store the move of the opponent
char   OppMove[5]       = "e2e4";
FILE*  LOGFILE;

// Setup parameters
char   *DGTPORT;

int    DGTORIENTATION = DGTNIX_BOARD_ORIENTATION_CLOCKLEFT;
int    DGTDEBUGMODE   = DGTNIX_DEBUG_OFF;

# define DGTNIX_CLOCK_PRESENT  0x01

int    DGTCLOCK       = DGTNIX_CLOCK_PRESENT;
int    WHITECLOCK     = -1;
int    BLACKCLOCK     = -1;
int    WHITESTURN     = -1;
int    ANNOUNCEMOVE   =  true;

// The delays for the reading of file descriptors
int    secs           = 5;             // seconds
int    usecs          = 0;             // micro-seconds

// these delays are used in setup mode. In setup mode only the
// flushing of external events has to occur, no moves are generated.
int    setupsecs      = 1;             // seconds
int    setupusecs     = 0;             // micro-seconds

//--------------------------------------------------------------------
// AnnounceMove()
// Announce the move by external program call
//
// char     : piece          : Character for the moving piece
// char     : *move          : the move as passed on to external
//--------------------------------------------------------------------
void AnnounceMove(char piece, char *move)
{
	char cmd[256];

	if (ANNOUNCEMOVE) {
		// Append & to fire the speak command to background,
		// use system() to call it via shell
		// Note: cmd should not contain \n!
		sprintf(cmd, "%s %c%s &\n", SPEAK, piece, move);
		system(cmd);
	}
}

//--------------------------------------------------------------------
// LogString()
// Write a string only to the log
//
// char     : *text          : String to write to the log, format...
//--------------------------------------------------------------------
void LogString(const char *text, ...)
{

	va_list ap;
	va_start(ap, text);
		vfprintf(LOGFILE, text, ap);
	va_end(ap);
	fflush(LOGFILE);
}

//--------------------------------------------------------------------
// WriteString()
// Write the string passed as is to the screen and the log. There may
// be several arguments passed as well as formatting string.
//
// char     : *text          : String to write to stdout, format...
//--------------------------------------------------------------------
void WriteString(const char *text, ...)
{

	va_list ap;
	va_start(ap, text);
		vfprintf(stdout, text, ap);
		vfprintf(LOGFILE, text, ap);
	va_end(ap);

	fflush(stdout);
	fflush(LOGFILE);
}

//--------------------------------------------------------------------
// WriteInfoString()
// Write the string passed as is to the screen and the log. There may
// be several arguments passed as well as formatting string.
// The string is prepended with info or info string depending on the
// engine mode.
//
// char     : *InfoStr       : String to write to stdout, format...
//--------------------------------------------------------------------
void WriteInfoString(const char *InfoStr, ...)
{
	if (MODE != NONE) 
		WriteString(MSG_INFOPREFIX);
	if ((MODE != XBOARD) && (MODE != NONE))
		WriteString(MSG_STRPREFIX);

	va_list ap;
	va_start(ap, InfoStr);
		vfprintf(stdout, InfoStr, ap);
		vfprintf(LOGFILE, InfoStr, ap);
	va_end(ap);

	fflush(stdout);
	fflush(LOGFILE);
}

//--------------------------------------------------------------------
// WriteInfoString()
// Write the string passed as is to the screen and the log. There may
// be several arguments passed as well as formatting string.  The
// string is prepended with info ERROR : or info string ERROR :
// depending on the engine mode.
//
// char     : *ErrStr        : String to write to stdout, format...
//--------------------------------------------------------------------
void WriteErrorString(const char *ErrStr, ...)
{
	WriteString(MSG_INFOPREFIX);
	if (MODE != XBOARD) {
		WriteString(MSG_STRPREFIX);
	}
	WriteString(MSG_ERRORPREFIX);

	va_list ap;
	va_start(ap, ErrStr);
		vfprintf(stdout, ErrStr, ap);
		vfprintf(LOGFILE, ErrStr, ap);
	va_end(ap);

	fflush(stdout);
	fflush(LOGFILE);
}

//--------------------------------------------------------------------
// WriteOpponentMove()
// Inform the user about the opponents move and if necessary announce
// it. The opponent move is passed in *move as computer notation that
// is the two changing fields from and to. The moving piece has to be
// retrieved from dgtnixGetBoard().
//
// char     : *move          : move of the oppontent (from to fields)
//--------------------------------------------------------------------
void WriteOpponentMove(char *move)
{
	const char *board = dgtnixGetBoard();

	int   file = int(move[0])-97; // a = 0 etc.
	int   rank = int(move[1])-49; // 1 = 0 etc.

	int   fieldnumber = file*8 + rank;
	char  piece = board[fieldnumber];

	if (piece > 91) {
		piece = piece - 32;
	}
		
	WriteInfoString(MSG_OPPONENTMOVE, move);
	AnnounceMove(piece, move);
}

//--------------------------------------------------------------------
// WriteMove()
// Depending on the engine mode the move is printed
//
// char     : piece          : Moving piece
// char     : *move          : move to write (from to fields)
//--------------------------------------------------------------------
void WriteMove(char piece, char *text)
{

	if ((MODE == XBOARD) || (MODE == CRAFTY)) {
		WriteString(MSG_MOVESTRING, text);
	}
	else if (MODE == UCI) {
		WriteString("\n%s\n",text);
	}
	AnnounceMove(piece, text);
}


//--------------------------------------------------------------------
// OpenLogFile(), CloseLogFile()
// These functions open and close the log file upon program
// startup and shutdown.
//--------------------------------------------------------------------
int OpenLogFile(void)
{
	LOGFILE = fopen(LogFileName, "w");
	if (!LOGFILE) {
		printf(MSG_ERRORPREFIX, MSG_UNABLETOOPEN, LogFileName);
		return (-1);
	}
	fprintf(LOGFILE, MSG_SESSIONSTART);
}

void CloseLogFile(void)
{
	fprintf(LOGFILE, MSG_SESSIONEND);
	fclose(LOGFILE);
}

//--------------------------------------------------------------------
// PrintLogo()
// Print out the logo plate as info strings.
//--------------------------------------------------------------------
void PrintLogo(void)
{
	WriteInfoString("---------------------------------------------\n");
	WriteInfoString("                                             \n");
	WriteInfoString("        /      /      /                      \n");
	WriteInfoString("    ___/ ___ _/_  ___/ /_- |  /        POSIX \n");
	WriteInfoString("   /  / /  / /   /  / /    | /        driver \n");
	WriteInfoString("  (__/ (__/ (_  (__/ /     |/        engine  \n");
	WriteInfoString("      ___/                                   \n");
	WriteInfoString("             for the DGT digital chess board \n");
	WriteInfoString("---------------------------------------------\n");
	WriteInfoString("(c) 2006-2007 by Alexander Wagner            \n");
	WriteInfoString("    %s\n", REVISIONTAG);
	WriteInfoString("---------------------------------------------\n");
}

//--------------------------------------------------------------------
// PrintUsageInfo()
// Gives usage info for the program call
//--------------------------------------------------------------------
void PrintUsageInfo(void)
{
	MODE = NONE;
	PrintLogo();
	WriteString("This program is a xboard/uci/crafty compatible\n");
	WriteString("chess engine. It is meant to be invoked from\n");
	WriteString("your chess GUI.\n\n");
	WriteString("\n");
	WriteString("Usage: dgtdrv <port> <options>\n\n");
	WriteString("<port>  : the com port your DGT Electronic Chess Board\n");
	WriteString("          is connected to\n");
	WriteString("          USB, try    : /dev/usb/tts/*, /dev/usbdev*\n");
	WriteString("          Serial, try : /dev/ttyS?\n");
	WriteString("          (These names apply to Linux.)\n");
	WriteString("<setup> : several characters defining the setup.\n");
	WriteString("          The syntax is identical to crafty, not POSIX.\n");
	WriteString("          The odering is significant here!\n");
	WriteString("          1st char: r, l\n");
	WriteString("              Position of the clock from Whites view.\n");
	WriteString("              r : right\n");
	WriteString("              l : left\n");
	WriteString("          2nd char: a, b, w\n");
	WriteString("              Moves to send as actual move.\n");
	WriteString("              a : all moves (analyse mode)\n");
	WriteString("              b : black moves only\n");
	WriteString("              w : white moves only\n");
	WriteString("<name>  : This argument is optional. It can contain\n");
	WriteString("          the name, the program uses to identify\n");
	WriteString("          itself to the GUI. Some GUIs use this string\n");
	WriteString("          for the PGN header fields.\n");
	WriteString("          No spaces are allowed here, quotes do not work!\n");

	WriteString("Example: $ dgtdrv /dev/dgt rw\n\n");
}

//--------------------------------------------------------------------
// PrintClockData()
// If there is a DGT Digital Chess Clock detected, print its current
// data and the side to move, otherwise just an informative string
// that there is no clock.
//--------------------------------------------------------------------
void PrintClockData(void)
{
	DGTCLOCK = dgtnixGetClockData(&WHITECLOCK, &BLACKCLOCK, &WHITESTURN);

	if (DGTCLOCK) {
		WriteInfoString(MSG_WHITETIME, WHITECLOCK);
		WriteInfoString(MSG_BLACKTIME, BLACKCLOCK);
		if (WHITESTURN) {
			WriteInfoString(MSG_WHITETOMOVE);
		}
		else {
			WriteInfoString(MSG_BLACKTOMOVE);
		}
	}
	else {
		// no clock connected
		WriteInfoString(MSG_NOCLOCK, WHITECLOCK);
	}
}

//--------------------------------------------------------------------
// PrintBoardInfo()
// Read in several technical parameters from the board and writes them
// to the screen and log as info-strings.
//--------------------------------------------------------------------
void PrintBoardInfo(void)
{
	WriteInfoString(MSG_CHESSBOARDFOUND);
	WriteInfoString(MSG_DGTNIXVERSION, 
			dgtnixQueryString(DGTNIX_DRIVER_VERSION));

	WriteInfoString(MSG_DGTTMSTRING,
			dgtnixQueryString(DGTNIX_TRADEMARK_STRING));

	WriteInfoString(MSG_DGTVERSION,
			dgtnixQueryString(DGTNIX_VERSION_STRING));
	WriteInfoString(MSG_DGTSERIALNO, 
			dgtnixQueryString(DGTNIX_SERIAL_STRING));
	WriteInfoString(MSG_DGTPORT, DGTPORT);
	WriteInfoString(MSG_DGTBUSADDRESS,
			dgtnixQueryString(DGTNIX_BUSADDRESS_STRING));

	// current mode of the engine
	switch (MODE) {
		case CRAFTY:
			WriteInfoString(MSG_CRAFTYMODE);
			break;
		case UCI:
			WriteInfoString(MSG_UCIMODE);
			break;
		case XBOARD:
			WriteInfoString(MSG_XBOARDMODE);
			break;
		default:
			break;
	}
	PrintClockData();
}

//--------------------------------------------------------------------
// PrintStatus()
// Dump the current setup data as far as it is not reported by either
// PrintBoardInfo() or PrintClockData().
//--------------------------------------------------------------------
void PrintStatus(void)
{
	WriteInfoString("# %s", PRGNAME);
	WriteString("\n");
	WriteInfoString("# by %s", PRGAUTHOR);
	WriteString("\n");


	// cycle debug mode
	switch (DGTDEBUGMODE) {
		case DGTNIX_DEBUG_OFF: 
			WriteInfoString(MSG_DEBUGLEVEL, MSG_OFF);
			break;
		case DGTNIX_DEBUG_ON: 
			WriteInfoString(MSG_DEBUGLEVEL, MSG_ON);
			break;
		case DGTNIX_DEBUG_WITH_TIME:
			WriteInfoString(MSG_DEBUGLEVEL, MSG_FULL);
			break;
		default:
			break;
	}

	switch (DGTMODE) {
		case SENDWHITE:
			WriteInfoString(MSG_SENDWHITE);
			break;
		case SENDBLACK:
			WriteInfoString(MSG_SENDBLACK);
			break;
		case SENDBOTH:
			WriteInfoString(MSG_SENDBOTH);
			break;
		default:
			break;
	}

	switch (DGTORIENTATION) {
		case DGTNIX_BOARD_ORIENTATION_CLOCKLEFT:
			WriteInfoString(MSG_CLOCKLEFT);
			break;
		case DGTNIX_BOARD_ORIENTATION_CLOCKRIGHT:
			WriteInfoString(MSG_CLOCKRIGHT);
			break;
		default:
			break;
	}

	WriteInfoString(MSG_MOVETIMING, secs, usecs);

	if (ANNOUNCEMOVE) {
		WriteInfoString(MSG_ANNOUNCE_ON);
	}
	else {
		WriteInfoString(MSG_ANNOUNCE_OFF);
	}
}

//--------------------------------------------------------------------
// ConnectDGT:
// Initialise connection to DGT Chess board and return file descriptor
// to read data from the board
//
// char     : *port          : Communication port to use
// int      : clockposition  : Clock to the left or right
//--------------------------------------------------------------------
int ConnectDGT(char *port, int clockposition)
{
	int BoardDescriptor;

	// Set debug mode and board orientation
	// Do this _BEFORE_ the call to init
	dgtnixSetOption(DGTNIX_DEBUG, DGTDEBUGMODE);
	dgtnixSetOption(DGTNIX_BOARD_ORIENTATION, clockposition);

	// open connection
	BoardDescriptor = dgtnixInit(port);

	if (BoardDescriptor < 0) {
		switch (BoardDescriptor) {
			case -1 : 
				WriteErrorString(MSG_UNABLEPORT, port);
				break;
			case -2 :
				WriteErrorString(MSG_NODGTBOARD);
				break;
			default:
				WriteErrorString(MSG_UNRECOGDGTRESPONSE);
		}
		return(-1);
	}
	else {
		// call the setup strings
		dgtnixQueryString(DGTNIX_TRADEMARK_STRING);
		dgtnixQueryString(DGTNIX_DRIVER_VERSION);
		dgtnixQueryString(DGTNIX_VERSION_STRING);
		dgtnixQueryString(DGTNIX_SERIAL_STRING);
		dgtnixQueryString(DGTNIX_BUSADDRESS_STRING);

		// wait for things to settle
		WriteInfoString(MSG_INITIALISING);
		usleep(5);
		DGTCLOCK = dgtnixGetClockData(&WHITECLOCK, &BLACKCLOCK, &WHITESTURN);

		return(BoardDescriptor);
	}
}

//--------------------------------------------------------------------
// DisplayBoard()
// Dumps a graphical version of the board as info strings.
// Meant primarily for debuging.
//--------------------------------------------------------------------
void DisplayBoard(void)
{
	const char *board = dgtnixGetBoard();

	WriteInfoString("  ------------------------------- \n");
	WriteInfoString(" |");

	for (int sq = 0; sq < 64; sq++)
	{
		WriteString(" %1c |", board[sq]);
		if ((sq+1) % 8 == 0) {
			WriteString(" %i \n", 8-sq / 8);
			WriteInfoString(" |---+---+---+---+---+---+---+---|\n");
			WriteInfoString(" |");
		}
	}
	WriteString(" a   b   c   d   e   f   g   h |\n");
	WriteInfoString(MSG_CURRENTBOARD);
}

//--------------------------------------------------------------------
// PrintFEN()
// Give the current board setup as FEN string
// char  :  tomove = 'w' or 'b' : the side to move
//
// char     : tomove         : the side to move (white is default)
//--------------------------------------------------------------------
void PrintFEN(char tomove = 'w')
{
	const char *board = dgtnixGetBoard();
	char  FEN[90];
	int   pos = 0;
	int   empty = 0;

	for (int sq = 0; sq < 64; sq++)
	{
		if (board[sq] != 32) {
			if (empty > 0) {
				FEN[pos] = empty+48;
				pos++;
				empty=0;
			}
			FEN[pos] = char(board[sq]);
			pos++;
		}
		else empty++;
		if ((sq+1) % 8 == 0) {
			if (empty > 0) {
				FEN[pos] = empty+48;
				pos++;
				empty=0;
			}
			if (sq < 63) {
				FEN[pos] = '/';
				pos++;
			}
			empty = 0;
		}
	}
	
	// FEN data fields
	FEN[pos++] = ' '; FEN[pos++] = tomove; // side to move
	FEN[pos++] = ' ';
	// possible castelings
	FEN[pos++] = 'K'; FEN[pos++] = 'Q';
	FEN[pos++] = 'k'; FEN[pos++] = 'q';
	FEN[pos++] = ' '; FEN[pos++] = '-';
	FEN[pos++] = ' '; FEN[pos++] = '0';
	FEN[pos++] = ' '; FEN[pos++] = '1';

	// Mark the end of the string
	FEN[pos] = char(0);

	WriteInfoString("FEN %s\n", FEN);
}

//--------------------------------------------------------------------
// PrintPosition()
// Give the current board setup as FEN string plus the Clock data
//
// char  :  tomove = 'w' or 'b' : the side to move
//--------------------------------------------------------------------
void PrintPosition(char tomove = 'w')
{
	PrintFEN(tomove);
	PrintClockData();
}

//--------------------------------------------------------------------
// CheckStartPosition()
// Check wether some sort of starting position is set up. A starting
// position is defined by all white pieces on the 1st rank (in any
// order to allow for chess960) all white pawns on the 2nd, all
// black pawns on the 7th and all black pieces in the 8th rank (in any
// order).
//--------------------------------------------------------------------
bool CheckStartPosition()
{
	const char *board = dgtnixGetBoard();
	bool  newgame = true;

	for (int i = 0; i < 64; i++) {
		if ( i <  8) {
			// check for only white pieces
			if ((board[i] > 64) && (board[i] < 91)) {
				newgame = false;
			}
		}
		if (( i >  7) && (i < 16)) {
			// check for only white pawns
			if (board[i] == 80) {
				newgame = false;
			}
		}
		if (( i > 15) && (i < 48)) {
			// check for empty squares
			if (board[i] != 32) {
				newgame = false;
			}
		}
		if (( i > 47) && (i < 56)) {
			// check for only black pawns
			if (board[i] == 112) {
				newgame = false;
			}
		}
		if ( i > 57)  {
			// check for only black pieces
			if ((board[i] > 96) && (board[i] < 123)) {
				newgame = false;
			}
		}
	}
	return(newgame);
}

//--------------------------------------------------------------------
// ReadDescriptor()
// Reads a file descriptor using read() but only waits for
// secs+usecs/1000 seconds for an event to occur. This avoids locking
// in a loop and going out of sync while processing input from the
// DGT.
// Drawback: The user might lift a piece and wait for more than the
// delay time to make his move. => no move will be recognised.
//
// int :  Descriptor : File descriptor to read
//--------------------------------------------------------------------
char *ReadDescriptor (int Descriptor)
{
	fd_set readfds;
	struct timeval tv;
	int    result, i;

	static char  buffer[4];
	char   buffer1[10];
	int    bytes;

	// add filehandle to event check
	FD_ZERO(&readfds);
	FD_SET(Descriptor, &readfds);

	// delay to wait for an event
	if (DGTMODE != SETUPMODE) {
		tv.tv_sec  = secs;
		tv.tv_usec = usecs;
	}
	else {
		tv.tv_sec  = setupsecs;
		tv.tv_usec = setupusecs;
	}

	// Check file handle for event
	result = select(32, &readfds, 0, 0, &tv);

	if (FD_ISSET(Descriptor, &readfds)) {
		bytes = read(Descriptor, buffer, 4);
		buffer[bytes] = 0;                         // terminate the string
	}
	else {
		buffer[0] = 0;
	}

	// disable events for setup mode and avoid
	// to confuse the move handler
	if (DGTMODE == SETUPMODE) {
		buffer[0] = 0;
	}

	return(buffer);
}

//--------------------------------------------------------------------
// ReadHalfMove()
// Call ReadDescriptor() to fetch the last message. Check if this is a
// clock message (identified by buffer[0] ==2), if so, print the clock
// data. Reread the descriptor till buffer[0] is either 0 or 1
// signifying a piece removed/placed.
//
// int :  Descriptor : File descriptor to read
//--------------------------------------------------------------------
char *ReadHalfMove (int Descriptor)
{

	char *buffer;       // return value from ReadDescriptor

	buffer = ReadDescriptor(Descriptor);
	if (buffer[0] == 2) {
		PrintClockData();
	}
	while((buffer[0] > 1) || (buffer[0] < 0)) {
		buffer = ReadDescriptor(Descriptor);
	}
	return(buffer);
}
 
//--------------------------------------------------------------------
// HandleDGT()
// Reads from BoardDescriptor and writes out a move made
//
// int   : BoardDescriptor     : Descriptor to read the DGT board
//
// DGT special moves:
//
// DONE - "move now"
//        Lift the white king and place it back on the same square
//        Extension: Handle the black king the same way
// DONE - "start setup"
//        Lift both kings from the board
// DONE - "end setup"
//        place the kings back on the board, the king of the side to move
//        is placed last.
// DONE - "end game"
//        both kings in the center of the board:
//        on white squares (e4, d5): 1 - 0
//        on black squares (e5, d4): 0 - 1
//        other                    : 1/2 - 1/2
// DONE - "new game"
//        all pawns on the second rank
//        all pieces on the first (in any order to support chess960)
//
//--------------------------------------------------------------------
int HandleDGT(int BoardDescriptor)
{
	char msg0[4];       // first half move
	char msg1[4];       // second half move
	char msg2[4];       // dummy for casteling
	char move[5];       // String with the actual move. 0-0-0 is 5 chars!
	char piece[3];      // piece that moves
	char text[50];      // a simple string
	char *buffer;       // return value from ReadHalfMove, copied to msg*
	const char *board;  // representation of board
	bool MoveWhitePiece = true;


	// clear move
	for (int i = 0; i < 5; i++) { move[i] = 0; }

	// read the board descriptor for the first half of the move
	buffer = ReadHalfMove(BoardDescriptor);
	for(int i=0; i < 4; i++) {
		msg0[i] = buffer[i];
	}

	// read board again for the second half of the move
	buffer = ReadHalfMove(BoardDescriptor);
	for(int i=0; i < 4; i++) {
		msg1[i] = buffer[i];
	}

	if (DGTDEBUGMODE > DGTNIX_DEBUG_OFF) {
		WriteInfoString("msg0: %s", msg0);
		WriteInfoString("msg1: %s", msg1);
	}


	// store the messages in internal format
	move[0]  = char(msg0[1]+32);
	move[1]  = char(msg0[2]+48);
	piece[0] = char(msg0[3]);

	move[2]  = char(msg1[1]+32);
	move[3]  = char(msg1[2]+48);
	piece[1] = char(msg1[3]);


	if (msg0[0] == DGTNIX_MSG_MV_REMOVE) {
		// a piece is lifted first = possible begin of a move

		if (msg1[0] == DGTNIX_MSG_MV_ADD) {
			// a piece is set second = possible end of a move

			//--------------------------------------------------
			// Check for an actual move
			//--------------------------------------------------
			if (piece[0] == piece[1]) {
				// piece lifted == piece set

				// Determine it's colour
				if ((piece[0] == 'P') || (piece[0] == 'R') ||
					 (piece[0] == 'N') || (piece[0] == 'B') ||
					 (piece[0] == 'Q') || (piece[0] == 'K') ) {
					MoveWhitePiece = true;
				}
				else {
					MoveWhitePiece = false;
				}

				//--------------------------------------------------
				// Some (non capture/promotion) move is detected
				//--------------------------------------------------
				if ((move[0] != move[2]) || (move[1] != move[3])) {
					// start and end square differ => normal move

					//--------------------------------------------------
					// Handle casteling
					//--------------------------------------------------
					if ((piece[0] == 'K') && 
						  (move[0] == 'e') && (move[1] == '1')) { 
						if ( (move[2] == 'g') && (move[3] == '1') ) {
							// white castles short
							if (MODE != UCI) {
								move[0] = '0';
								move[1] = '-';
								move[2] = '0';
								move[3] = 0;
								// read the rook move and trow it away
								buffer = ReadHalfMove(BoardDescriptor);
								buffer = ReadHalfMove(BoardDescriptor);
								for(int i=0; i < 4; i++) {
									msg2[i] = buffer[i];
								}
							}
						}
						else if ( (move[2] == 'c') && (move[3] == '1') ) {
							// white castles long
							if (MODE != UCI) {
								move[0] = '0';
								move[1] = '-';
								move[2] = '0';
								move[3] = '-';
								move[4] = '0';
								move[5] = 0;
								// read the rook move and trow it away
								buffer = ReadHalfMove(BoardDescriptor);
								buffer = ReadHalfMove(BoardDescriptor);
								for(int i=0; i < 4; i++) {
									msg2[i] = buffer[i];
								}
							}
						}
					} // detect white casteling

					if ((piece[0] == 'k') && 
						  (move[0] == 'e') && (move[1] == '8')) { 
						if ( (move[2] == 'g') && (move[3] == '8') ) {
							// black castles short
							if (MODE != UCI) {
								move[0] = '0';
								move[1] = '-';
								move[2] = '0';
								move[3] = 0;
								buffer = ReadHalfMove(BoardDescriptor);
								buffer = ReadHalfMove(BoardDescriptor);
								for(int i=0; i < 4; i++) {
									msg2[i] = buffer[i];
								}
							}
						}
						else if ( (move[2] == 'c') && (move[3] == '8') ) {
							// black castles long
							if (MODE != UCI) {
								move[0] = '0';
								move[1] = '-';
								move[2] = '0';
								move[3] = '-';
								move[4] = '0';
								move[5] = 0;
								buffer = ReadHalfMove(BoardDescriptor);
								buffer = ReadHalfMove(BoardDescriptor);
								for(int i=0; i < 4; i++) {
									msg2[i] = buffer[i];
								}
							}
						}
					} // detect black casteling

					//--------------------------------------------------
					// A move was recognised: probably send it
					//--------------------------------------------------
					if (DGTMODE != SETUPMODE) {
						if (   (DGTMODE == SENDBOTH)                        ||
								((DGTMODE == SENDWHITE) &&  (MoveWhitePiece)) ||
								((DGTMODE == SENDBLACK) && !(MoveWhitePiece))
							) {
							// if not in setup mode or the wrong side moves
							// send the move as normal move command
							WriteInfoString(MSG_MOVINGPIECE, piece[0]);
							WriteMove(piece[0], move);
						}
						else {
							if (!(DGTMODE == SENDBOTH) ) {
								// compare the move made with the opponent
								// move signaled by the opposing engine
								if ((move[0] != OppMove[0]) || (move[1] != OppMove[1]) ||
									 (move[2] != OppMove[2]) || (move[3] != OppMove[3])
									) {
									WriteInfoString(MSG_WRONGOPPONENTMOVE, move, OppMove);
								}
							}
							WriteInfoString(MSG_MOVINGPIECE, piece[0]);
							WriteString("\n");
							WriteInfoString("move %s\n", move);
						}
					} // setup mode check

					//--------------------------------------------------
					// King move: might be final result (1-0, 0-1, 1/2-1/2)
					//--------------------------------------------------
					if ( (piece[0] == 'K') || (piece[0] == 'k') )
					{

					// NEEDS REWORKING: xboard gets confused by the two
					// king moves as one of them can be invalid (more than
					// one field) giving "forfait due to illegal move"

						// retrieve the board for comparison
						board = dgtnixGetBoard();
						// 27: d4-b, 28: e4-w, 35: d5-w, 36: e5-b
						if (board[27] == 'k') {        // d4: black
							if (board[28] == 'K') {     // e4: white
								WriteInfoString(MSG_ENDGAME11);
							}
							if (board[35] == 'K') {     // d5: white
								WriteInfoString(MSG_ENDGAME11);
							}
							if (board[36] == 'K') {     // e5: black
								WriteInfoString(MSG_ENDGAME10);
							}
						}
						if (board[28] == 'k') {        // e4: white
							if (board[27] == 'K') {     // d4: black
								WriteInfoString(MSG_ENDGAME11);
							}
							if (board[35] == 'K') {     // d5: white
								WriteInfoString(MSG_ENDGAME01);
							}
							if (board[36] == 'K') {     // e5: black
								WriteInfoString(MSG_ENDGAME11);
							}
						}
						if (board[35] == 'k') {        // d5: white
							if (board[27] == 'K') {     // d4: black
								WriteInfoString(MSG_ENDGAME11);
							}
							if (board[28] == 'K') {     // e4: white
								WriteInfoString(MSG_ENDGAME01);
							}
							if (board[36] == 'K') {     // e5: black
								WriteInfoString(MSG_ENDGAME11);
							}
						}
						if (board[36] == 'k') {        // e5: black
							if (board[27] == 'K') {     // d4: black
								WriteInfoString(MSG_ENDGAME10);
							}
							if (board[28] == 'K') {     // d5: white
								WriteInfoString(MSG_ENDGAME11);
							}
							if (board[35] == 'K') {     // d5: white
								WriteInfoString(MSG_ENDGAME11);
							}
						}
					}
				} // normal move detected

				//--------------------------------------------------
				// King was lifted and set back: "move now!"
				//--------------------------------------------------
				else if ((piece[0] == 'K') || (piece[0] == 'k')) {
					// either king was lifted and set back => move now!
					WriteInfoString(MSG_MOVENOW);
				} // king lifted and set back
			} // both pieces are the same

			//--------------------------------------------------
			// Handle promotion: a pawn is lifted from the 7th 
			// rank and some other piece is set to the 8th.
			// NOTE: this code does not ensure that the user 
			// places a new piece of the correct colour! 
			// As usual this has to be checked from the GUI.
			//--------------------------------------------------
			if ((piece[0] == 'P') && (move[1] == '7') && (move[3] == '8')) {
				sprintf(text, "%c%c%c%c=%c",
						move[0],move[1],move[2], move[3], piece[1]);
				WriteMove(piece[0], text);
			}
			if  ((piece[0] == 'p') && (move[1] == '2') && (move[3] == '1')) {
				sprintf(text, "%c%c%c%c=%c",
						move[0],move[1],move[2], move[3], piece[1]-32);
				WriteMove(piece[0], text);
			}
		} // second piece set

		else if (msg1[0] == DGTNIX_MSG_MV_REMOVE) {
			// two pieces were removed => Enter setup mode?

			//--------------------------------------------------
			// Both kings were removed: Enter setup mode
			//--------------------------------------------------
			if (((piece[0] == 'K') && (piece[1]=='k')) ||
				 ((piece[0] == 'k') && (piece[1]=='K'))) {
					WriteInfoString(MSG_STARTSETUP);
					DGTINITIALMODE = DGTMODE;
					DGTMODE = SETUPMODE;
			} // setup mode entered

			//--------------------------------------------------
			// Handle captures
			// Capturing piece is lifted, then taken piece
			// removed and capturing piece set to the new field
			// -> If the captured piece is removed first, a normal
			// move will be recognised. That is perfectly OK!
			//--------------------------------------------------
			if ((piece[1] != 'K') && (piece[1] != 'k')) {
				// can't take the king...

				//---// buffer = ReadDescriptor(BoardDescriptor);
				buffer = ReadHalfMove(BoardDescriptor);
				for(int i=0; i < 4; i++) {
					msg2[i] = buffer[i];
				}

				move[2]  = char(msg2[1]+32);
				move[3]  = char(msg2[2]+48);
				piece[2] = char(msg2[3]);
				if (piece[0] == piece[2]) {

					WriteInfoString(MSG_TAKES, piece[0], piece[1]);

					// Determine it's colour
					if ((piece[0] == 'P') || (piece[0] == 'R') ||
						 (piece[0] == 'N') || (piece[0] == 'B') ||
						 (piece[0] == 'Q') || (piece[0] == 'K') ) {
						MoveWhitePiece = true;
					}
					else {
						MoveWhitePiece = false;
					}

					// Check if the right colour is moving
					if (   (DGTMODE == SENDBOTH)                        ||
							((DGTMODE == SENDWHITE) &&  (MoveWhitePiece)) ||
							((DGTMODE == SENDBLACK) && !(MoveWhitePiece))
						) {
						WriteMove(piece[0], move);
					}
					else {
						WriteString("\n");
						WriteInfoString("move %s\n", move);
					}
				}
			}
		} // second pieces lifted
	} // first piece is lifted
	else if(msg0[0] == DGTNIX_MSG_MV_ADD) {
		// a piece was added first

		if (msg1[0] == DGTNIX_MSG_MV_ADD) {
			// a pieces was added second

			//--------------------------------------------------
			// Set back the Kings: Leave setup mode
			//--------------------------------------------------
			if (((piece[0] == 'K') && (piece[1]=='k')) ||
				 ((piece[0] == 'k') && (piece[1]=='K'))) {

				bool newgame = CheckStartPosition();

				WriteInfoString(MSG_ENDSETUP);
				if (newgame) {
					WriteInfoString(MSG_NEWGAME);
					// to get white to move below:
					piece[1] = 'K';
				}
				// decide which side has the move
				if (piece[1] == 'K') {
					WriteInfoString(MSG_WHITETOMOVE);
					PrintPosition('w');
					DGTMODE = DGTINITIALMODE;
				}
				else {
					WriteInfoString(MSG_BLACKTOMOVE);
					PrintPosition('b');
					DGTMODE = DGTINITIALMODE;
				}
			} // leave setup mode
		} // second piece added
	} // first piece added
}

//--------------------------------------------------------------------
// HandleGUI()
// GUI-commands come from stdin
//
// int   : CMDDescriptor : Descriptor to read stdin from
//--------------------------------------------------------------------
bool HandleGUI(int CMDDescriptor)
{
	int bytes;
	char buffer[128], *cmd, *newline;

	bytes = read(CMDDescriptor, buffer, 128);  // read the command
	buffer[bytes] = 0;                         // terminate the string
	cmd = buffer;                              // for strcmp

	LogString(MSG_READSTDIN, LOGINPUT, bytes, cmd);

	//--------------------------------------------------
	// crafty mode
	//--------------------------------------------------

	// crafty: switch to crafty mode
	if (strstr(cmd, "crafty")) {
		MODE = CRAFTY;
		WriteInfoString(MSG_CRAFTYMODE);
	}

	//--------------------------------------------------
	// xboard mode
	//--------------------------------------------------

	// xboard: swith to xboard mode, initiate session
	else if (strstr(cmd, "xboard")) {
		MODE = XBOARD;
		WriteInfoString(MSG_XBOARDMODE);
	}

	// protover: only for xboard protocol 2
	if (strstr(cmd, "protover")) {
		// disable all features as this "engine" can do almost nothing
		// except sending moves. For protover < 2 this command is not
		// sent so it does no harm, for > 2 new features should default
		// to "disabled".
		// Do NOT allow sigint! We are using serial connections here!
		WriteString("feature ping=0 setboard=0 playother=0 san=0 pause=1\n");
		WriteString("feature usermove=0 time=0 draw=1 sigint=0 sigterm=1\n");
		WriteString("feature reuse=1 analyze=0 colors=0 ics=0 name=0\n");
		WriteString("feature myname=\"%s\"\n", PRGNAME);
		WriteString("feature done=1\n");
	}
	else if (strstr(cmd, "new")) {
		PrintPosition();
	}

	//--------------------------------------------------
	// uci mode
	//--------------------------------------------------

	// uci: initiate session
	if (strstr(cmd, "uci")) { 
		MODE = UCI;
		WriteString("id name %s\n", PRGNAME);
		WriteString("id author %s\n", PRGAUTHOR);
		WriteString("uciok\n");
	}

	// ucinewgame: start a new game
	else if (strstr(cmd, "ucinewgame")) {
		PrintPosition();
	}

	// isready: return ok to the calling program
	else if (strstr(cmd, "isready")) {
		WriteString("readyok\n");
	}

	//--------------------------------------------------
	// general commands
	//--------------------------------------------------
//	if (strstr(cmd, "reset")) { 
//		WriteInfoString(MSG_NOTIMPLEMENTED, "reset");
//	}

	else if (strstr(cmd, "logo")) { 
		PrintLogo();
	}

	// sysinfo: get all internal information
	else if (strstr(cmd, "sysinfo")) { 
		PrintBoardInfo();
		PrintClockData();
		PrintStatus();
	}

	// display: graphical display of the board
	else if (strstr(cmd, "display")) { 
		DisplayBoard();
	}

	// getposition: return position in FEN
	else if (strstr(cmd, "getposition")) { 
		PrintFEN();
	}

	else if (strstr(cmd, "checkforclock")) { 
		PrintClockData();
	}

	// getclock: return DGT Digital Clock status 
	else if (strstr(cmd, "getclock")) { 
		PrintClockData();
	}

	// rotateboard: rotate the board
	else if (strstr(cmd, "rotateboard")) { 
		if (DGTORIENTATION == DGTNIX_BOARD_ORIENTATION_CLOCKRIGHT) {
			DGTORIENTATION = DGTNIX_BOARD_ORIENTATION_CLOCKLEFT;
			WriteInfoString(MSG_CLOCKLEFT);
		}
		else {
			DGTORIENTATION = DGTNIX_BOARD_ORIENTATION_CLOCKRIGHT;
			WriteInfoString(MSG_CLOCKRIGHT);
		}
		dgtnixSetOption(DGTNIX_BOARD_ORIENTATION, DGTORIENTATION);
		PrintPosition();
	}

	// +/-, >/<: Change the detection sensitivity
	else if (strstr(cmd, "+")) { 
		secs++;
	}
	else if (strstr(cmd, "-")) { 
		if (secs > 2) {
			secs--;
		}
	}
	else if (strstr(cmd, ">")) { 
		usecs += 10;
	}
	else if (strstr(cmd, "<")) { 
		if (usecs > 10) {
			usecs -= 10;
		}
	}

	// DEGUG: cycle debug mode
	else if (strstr(cmd, "DEBUG")) { 
		switch (DGTDEBUGMODE) {
			case DGTNIX_DEBUG_OFF: 
				DGTDEBUGMODE = DGTNIX_DEBUG_ON;
				break;
			case DGTNIX_DEBUG_ON: 
				DGTDEBUGMODE = DGTNIX_DEBUG_WITH_TIME;
				break;
			case DGTNIX_DEBUG_WITH_TIME:
				DGTDEBUGMODE = DGTNIX_DEBUG_OFF;
				break;
			default:
				DGTDEBUGMODE = DGTNIX_DEBUG_ON;
				break;
		}
		dgtnixSetOption(DGTNIX_DEBUG, DGTDEBUGMODE);
	}

	// sendwhite: send only white moves
	else if (strstr(cmd, "sendwhite") || strstr(cmd, "white")) { 
		DGTMODE = SENDWHITE;
		WriteInfoString(MSG_SENDWHITE);
	}

	// sendblack: send only black moves
	else if (strstr(cmd, "sendblack") || strstr(cmd, "black")) { 
		DGTMODE = SENDBLACK;
		WriteInfoString(MSG_SENDBLACK);
	}

	// sendboth: send white and black moves (analysis mode)
	else if (strstr(cmd, "sendboth")) { 
		DGTMODE = SENDBOTH;
		WriteInfoString(MSG_SENDBOTH);
	}

	// If "go" command is recieved: print external clock
	// to get in sync (if a clock is connected)
	else if (strstr(cmd, "go")) { 
		PrintClockData();
	}

	// Switch move accoutsic announcements
	// to get in sync (if a clock is connected)
	else if (strstr(cmd, "announce")) { 

		ANNOUNCEMOVE = !(ANNOUNCEMOVE);
		if (ANNOUNCEMOVE) {
			WriteInfoString(MSG_ANNOUNCE_ON);
		}
		else {
			WriteInfoString(MSG_ANNOUNCE_OFF);
		}
	}

	// exit/quit/:q: end program
	else if (strstr(cmd, "exit") ||
		 strstr(cmd, "quit") ||
		 strstr(cmd, ":q"  )  ) {
		return(false);
	}

	// check if we just got a move from the opponent, then store it for
	// comparison with the move made on the board
	else if ((cmd[0] > int('a')) && (cmd[0] < int('h')) &&
				(cmd[1] > int('1')) && (cmd[1] < int('8')) &&
				(cmd[2] > int('a')) && (cmd[2] < int('h')) &&
				(cmd[3] > int('1')) && (cmd[3] < int('8'))
			) {
			for (int i = 0; i < 4; i++) {
				OppMove[i] = cmd[i];
			}
			// write OppMove strips also the \n in the cmd-string
			WriteOpponentMove(OppMove);
	}
	else {
		// something we do not know a thning how to handle
	}

	return(true);
}

//--------------------------------------------------------------------
// ParseCommandLine()
// Parse arguments passed on program startup.
// NOTE: The syntax is _NOT_ POSIX compatible but is the one used by
// crafty in it's dgtdrv-implementation. POSIX compatiblity is spoiled
// here to keep crafty happy.
//
// int   :   argc : number of arguments
// char  : **argv : parameter list
//--------------------------------------------------------------------
void ParseCommandLine(int argc, char **argv) 
{
	DGTPORT = argv[1];

	// r/l: the board orientation
	if (argv[2][0] == 'r' ) {
		DGTORIENTATION = DGTNIX_BOARD_ORIENTATION_CLOCKRIGHT;
	}
	if (argv[2][0] == 'l' ) {
		DGTORIENTATION = DGTNIX_BOARD_ORIENTATION_CLOCKLEFT;
	}

	// a/w/b: send all, white, black moves
	if (argv[2][1] == 'a' ) {
		DGTMODE = SENDBOTH;
	}
	if (argv[2][1] == 'w' ) {
		DGTMODE = SENDWHITE;
	}
	if (argv[2][1] == 'b' ) {
		DGTMODE = SENDBLACK;
	}

	if (argc > 3) {
		PRGNAME = argv[3];
	}
}

//====================================================================
// main()
//====================================================================
int main(int argc, char **argv) 
{
	int BoardDescriptor;
	int CMDDescriptor = fileno(stdin);
	bool run = true;

	fd_set readfds;
	struct timeval tv;
	int    result, i;

	OpenLogFile();

	if (argc < 2)
		PrintUsageInfo();
	else {
		ParseCommandLine(argc, argv);
		PrintLogo();
		BoardDescriptor = ConnectDGT(DGTPORT, DGTORIENTATION);

		if (BoardDescriptor > 0) {
			// DGT Initialisation succeded
			PrintBoardInfo();
			PrintPosition('w');

			while(run) {
				// add both filehandles to event check
				FD_ZERO(&readfds);
				FD_SET(BoardDescriptor, &readfds);
				FD_SET(CMDDescriptor,   &readfds);

				// delay to wait for an event
				tv.tv_sec  = secs;
				tv.tv_usec = usecs;

				// Check both file handles for events
				result = select(32, &readfds, 0, 0, &tv);

				if (FD_ISSET(BoardDescriptor, &readfds)) {
					// There was a board event
					if (DGTDEBUGMODE > DGTNIX_DEBUG_OFF) {
						WriteInfoString("-----BARD EVENT----\n");
					}
					HandleDGT(BoardDescriptor);
				}
				if (FD_ISSET(CMDDescriptor, &readfds)) {
					// There was a CMD event
					if (DGTDEBUGMODE > DGTNIX_DEBUG_OFF) {
						WriteInfoString("-----CMD  EVENT----\n");
					}
					run = HandleGUI(CMDDescriptor);
				}
			}
		}
	}
	dgtnixClose();
	CloseLogFile();
	return(0);
}

