/*
 * $Id: wmenu.c,v 1.28 2016/03/29 19:01:29 markisch Exp $Id: wmenu.c,v 1.27 2014/04/03 00:36:17 markisch Exp $
 */

/* GNUPLOT - win/wmenu.c */
/*[
 * Copyright 1992, 1993, 1998, 2004   Maurice Castro, Russell Lang
 *
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.
 *
 * Permission to modify the software is granted, but not the right to
 * distribute the complete modified source code.  Modifications are to
 * be distributed as patches to the released version.  Permission to
 * distribute binaries produced by compiling modified sources is granted,
 * provided you
 *   1. distribute the corresponding source modifications from the
 *    released version in the form of a patch file along with the binaries,
 *   2. add special version identification to distinguish your version
 *    in addition to the base release version number,
 *   3. provide your name and address as the primary contact for the
 *    support of your modified version, and
 *   4. retain our contact information in regard to use of the base
 *    software.
 * Permission to distribute the released version of the source code along
 * with corresponding source modifications in the form of a patch file is
 * granted with same provisions 2 through 4 for binary distributions.
 *
 * This software is provided "as is" without express or implied warranty
 * to the extent permitted by applicable law.
   ]*/

/*
 * AUTHORS
 *
 *   Maurice Castro
 *   Russell Lang
 *
 */
#define COBJMACROS
#define CINTERFACE
#include <gnuplot.h>
#pragma hdrstop
#define STRICT
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shlobj.h>
#include <winuser.h>
// @sobolev #include <shlwapi.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "wgnuplib.h"
#include "wresourc.h"
#include "wcommon.h"
#include <objidl.h> // @sobolev

/* limits */
#define MAXSTR 255
#define MACROLEN 10000
/* #define NUMMENU 256  defined in wresourc.h */
#define MENUDEPTH 3

/* menu tokens */
#define CMDMIN 129
#define INPUT 129
#define EOS 130
#define OPEN 131
#define SAVE 132
#define DIRECTORY 133
#define OPTIONS 134
#define ABOUT 135
#define CMDMAX 135
char * keyword[] = {
	"[INPUT]", "[EOS]", "[OPEN]", "[SAVE]", "[DIRECTORY]",
	"[OPTIONS]", "[ABOUT]",
	"{ENTER}", "{ESC}", "{TAB}",
	"{^A}", "{^B}", "{^C}", "{^D}", "{^E}", "{^F}", "{^G}", "{^H}",
	"{^I}", "{^J}", "{^K}", "{^L}", "{^M}", "{^N}", "{^O}", "{^P}",
	"{^Q}", "{^R}", "{^S}", "{^T}", "{^U}", "{^V}", "{^W}", "{^X}",
	"{^Y}", "{^Z}", "{^[}", "{^\\}", "{^]}", "{^^}", "{^_}",
	NULL
};
BYTE keyeq[] = {
	INPUT, EOS, OPEN, SAVE, DIRECTORY,
	OPTIONS, ABOUT,
	13, 27, 9,
	1, 2, 3, 4, 5, 6, 7, 8,
	9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 28, 29, 30, 31,
	0
};

#define GBUFSIZE 512
typedef struct tagGFILE {
	HFILE hfile;
	char getbuf[GBUFSIZE];
	int getnext;
	int getleft;
} GFILE;

static GFILE * Gfopen(LPSTR lpszFileName, int fnOpenMode);
static void Gfclose(GFILE * gfile);
static int Gfgets(LPSTR lp, int size, GFILE * gfile);
static int GetLine(char * buffer, int len, GFILE * gfile);
static void LeftJustify(char * d, char * s);
static BYTE MacroCommand(LPTW lptw, UINT m);
static void TranslateMacro(char * string);
INT_PTR CALLBACK InputBoxDlgProc(HWND, UINT, WPARAM, LPARAM);
INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData);
//
// Note: this code has been bluntly copied from MSDN article KB179378
// "How To Browse for Folders from the Current Directory"
//
INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
	TCHAR szDir[MAX_PATH];

	switch(uMsg) {
		case BFFM_INITIALIZED:
		    if(GetCurrentDirectory(sizeof(szDir)/sizeof(TCHAR), szDir)) {
			    // WParam is true since you are passing a path. It would be false if you were passing a pidl.
			    SendMessage(hwnd, BFFM_SETSELECTION, true, (LPARAM)szDir);
		    }
		    break;

		case BFFM_SELCHANGED:
		    // Set the status window to the currently selected path.
		    if(SHGetPathFromIDList((LPITEMIDLIST)lp, szDir)) {
			    SendMessage(hwnd, BFFM_SETSTATUSTEXT, 0, (LPARAM)szDir);
		    }
		    break;
	}
	return 0;
}

BYTE MacroCommand(LPTW lptw, UINT m)
{
	BYTE * s = lptw->lpmw->macro[m];
	while(s && *s) {
		if((*s >= CMDMIN) && (*s <= CMDMAX))
			return *s;
		s++;
	}
	return 0;
}

/* Send a macro to the text window */
void SendMacro(LPTW lptw, UINT m)
{
	BYTE * s;
	char * d;
	BOOL flag = true;
	int i;
	LPMW lpmw = lptw->lpmw;
	OPENFILENAME ofn;
	char * szTitle;
	char * szFile;
	char * szFilter;
	char * buf = (char *)LocalAllocPtr(LHND, MAXSTR+1);
	if(buf == 0)
		return;
	if((int)m >= lpmw->nCountMenu)
		return;
	s = lpmw->macro[m];
	d = buf;
	*d = '\0';
	while(s && *s && (d-buf < MAXSTR)) {
		if(*s>=CMDMIN  && *s<=CMDMAX) {
			switch(*s) {
				case SAVE: /* [SAVE] - get a save filename from a file list box */
				case OPEN: /* [OPEN] - get a filename from a file list box */
			    {
				    BOOL save;
				    char cwd[MAX_PATH];
				    if( (szTitle = (char *)LocalAllocPtr(LHND, MAXSTR+1)) == (char*)NULL)
					    return;
				    if( (szFile = (char *)LocalAllocPtr(LHND, MAXSTR+1)) == (char*)NULL)
					    return;
				    if( (szFilter = (char *)LocalAllocPtr(LHND, MAXSTR+1)) == (char*)NULL)
					    return;

				    save = (*s==SAVE);
				    s++;
				    for(i = 0; (*s >= 32 && *s <= 126); i++)
					    szTitle[i] = *s++;                  /* get dialog box title */
				    szTitle[i] = '\0';
				    s++;
				    for(i = 0; (*s >= 32 && *s <= 126); i++)
					    szFile[i] = *s++;                   /* temporary copy of filter */
				    szFile[i++] = '\0';
				    lstrcpy(szFilter, "Default (");
				    lstrcat(szFilter, szFile);
				    lstrcat(szFilter, ")");
				    i = lstrlen(szFilter);
				    i++; /* move past NULL */
				    lstrcpy(szFilter+i, szFile);
				    i += lstrlen(szFilter+i);
				    i++; /* move past NULL */
				    lstrcpy(szFilter+i, "All Files (*.*)");
				    i += lstrlen(szFilter+i);
				    i++; /* move past NULL */
				    lstrcpy(szFilter+i, "*.*");
				    i += lstrlen(szFilter+i);
				    i++; /* move past NULL */
				    szFilter[i++] = '\0'; /* add a second NULL */
				    flag = 0;

				    szFile[0] = '\0';
				    /* clear the structrure */
				    memzero(&ofn, sizeof(OPENFILENAME));
				    ofn.lStructSize = sizeof(OPENFILENAME);
				    ofn.hwndOwner = lptw->hWndParent;
				    ofn.lpstrFilter = szFilter;
				    ofn.nFilterIndex = 1;
				    ofn.lpstrFile = szFile;
				    ofn.nMaxFile = MAXSTR;
				    ofn.lpstrFileTitle = szFile;
				    ofn.nMaxFileTitle = MAXSTR;
				    ofn.lpstrTitle = szTitle;
				    /* Windows XP has it's very special meaning of 'default directory'  */
				    /* (search for OPENFILENAME on MSDN). So we set it here explicitly: */
				    /* ofn.lpstrInitialDir = (LPSTR)NULL; */
				    _getcwd(cwd, sizeof(cwd));
				    ofn.lpstrInitialDir = cwd;
				    ofn.Flags = OFN_SHOWHELP | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
				    flag = (save ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn));
				    if(flag) {
					    lpmw->nChar = lstrlen(ofn.lpstrFile);
					    for(i = 0; i<lpmw->nChar; i++)
						    *d++ = ofn.lpstrFile[i];
				    }

				    LocalFreePtr((void NEAR*)szTitle);
				    LocalFreePtr((void NEAR*)szFilter);
				    LocalFreePtr((void NEAR*)szFile);
			    }
			    break;

				case INPUT: /* [INPUT] - input a string of characters */
				    s++;
				    for(i = 0; (*s >= 32 && *s <= 126); i++)
					    lpmw->szPrompt[i] = *s++;
				    lpmw->szPrompt[i] = '\0';
				    flag = DialogBox(hdllInstance, "InputDlgBox", lptw->hWndParent, InputBoxDlgProc);
				    if(flag) {
					    for(i = 0; i<lpmw->nChar; i++)
						    *d++ = lpmw->szAnswer[i];
				    }
				    break;

				case DIRECTORY: /* [DIRECTORY] - show standard directory dialog */
			    {
				    BROWSEINFO bi;
				    LPITEMIDLIST pidl;
				    /* allocate some space */
				    if( (szTitle = (char *)LocalAllocPtr(LHND, MAXSTR+1)) == (char*)NULL)
					    return;

				    /* get dialog box title */
				    s++;
				    for(i = 0; (*s >= 32 && *s <= 126); i++)
					    szTitle[i] = *s++;
				    szTitle[i] = '\0';

				    flag = 0;

				    /* The Shell's internal directory chooser:
				     */
				    /* Note: This code does not work NT 3.51 and Win32s
				                     Windows 95 has shell32.dll version 4.0, but does not
				             have a version number, so this will return false.
				     */
				    /* Make sure that the installed shell version supports this approach */
				    if(GetDllVersion(TEXT("shell32.dll")) >= PACKVERSION(4, 0)) {
					    ZeroMemory(&bi, sizeof(bi));
					    bi.hwndOwner = lptw->hWndParent;
					    bi.pidlRoot = NULL;
					    bi.pszDisplayName = NULL;
					    bi.lpszTitle = szTitle;
					    /* BIF_NEWDIALOGSTYLE is supported by Win 2000 or later (Version 5.0)*/
					    bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_EDITBOX | BIF_STATUSTEXT | BIF_RETURNONLYFSDIRS | BIF_RETURNFSANCESTORS;
					    bi.lpfn = BrowseCallbackProc;
					    bi.lParam = 0;
					    bi.iImage = 0;
					    pidl = SHBrowseForFolder(&bi);
					    if(pidl != NULL) {
						    LPMALLOC pMalloc;
						    char szPath[MAX_PATH];
						    // Convert the item ID list's binary representation into a file system path
						    SHGetPathFromIDList(pidl, szPath);
						    uint len = strlen(szPath);
						    flag = len > 0;
						    if(flag)
							    for(i = 0; i < (int)len; i++)
								    *d++ = szPath[i];
						    // Allocate a pointer to an IMalloc interface Get the address of our task allocator's IMalloc interface
						    SHGetMalloc(&pMalloc);
						    IMalloc_Free(pMalloc, pidl); // Free the item ID list allocated by SHGetSpecialFolderLocation
						    IMalloc_Release(pMalloc); // Free our task allocator
					    }
				    }
				    else {
					    strcpy(lpmw->szPrompt, szTitle);
					    flag = DialogBox(hdllInstance, "InputDlgBox", lptw->hWndParent, InputBoxDlgProc);
					    if(flag) {
						    for(i = 0; i<lpmw->nChar; i++)
							    *d++ = lpmw->szAnswer[i];
					    }
				    }
				    LocalFreePtr((void NEAR*)szTitle);
			    }
			    break;

				case OPTIONS: { /* [OPTIONS] - open popup menu */
				    POINT pt;
				    RECT rect;
				    int index;
				    s++;
				    /* align popup with toolbar button */
				    index = lpmw->nButton - (lpmw->nCountMenu - m);
				    if(SendMessage(lpmw->hToolbar, TB_GETITEMRECT, (WPARAM)index, (LPARAM)&rect)) {
					    pt.x = rect.left;
					    pt.y = rect.bottom + 1;
					    ClientToScreen(lptw->hWndParent, &pt);
				    }
				    else {
					    GetCursorPos(&pt);
				    }
				    TrackPopupMenu(lptw->hPopMenu, TPM_LEFTALIGN | TPM_TOPALIGN,
					    pt.x, pt.y, 0, lptw->hWndParent, NULL);
				    break;
			    }

				case ABOUT: /* [ABOUT] - display About box */
				    s++;
				    SendMessage(lptw->hWndText, WM_COMMAND, M_ABOUT, (LPARAM)0);
				    break;

				case EOS: /* [EOS] - End Of String - do nothing */
				default:
				    s++;
				    break;
			}
			if(!flag) { /* abort */
				d = buf;
				s = (BYTE*)"";
			}
		}
		else {
			*d++ = *s++;
		}
	}
	*d = '\0';
	if(buf[0]!='\0') {
		d = buf;
		while(*d) {
			SendMessage(lptw->hWndText, WM_CHAR, *d, 1L);
			d++;
		}
	}
}

static GFILE * Gfopen(LPSTR lpszFileName, int fnOpenMode)
{
	GFILE * gfile;

	gfile = (GFILE*)LocalAllocPtr(LHND, sizeof(GFILE));
	if(!gfile)
		return NULL;

	gfile->hfile = _lopen(lpszFileName, fnOpenMode);
	if(gfile->hfile == HFILE_ERROR) {
		LocalFreePtr((void NEAR*)gfile);
		return NULL;
	}
	gfile->getleft = 0;
	gfile->getnext = 0;
	return gfile;
}

static void Gfclose(GFILE * gfile)
{
	_lclose(gfile->hfile);
	LocalFreePtr((void NEAR*)gfile);
	return;
}

/* returns number of characters read */
static int Gfgets(LPSTR lp, int size, GFILE * gfile)
{
	int i;
	int ch;
	for(i = 0; i<size; i++) {
		if(gfile->getleft <= 0) {
			if( (gfile->getleft = _lread(gfile->hfile, gfile->getbuf, GBUFSIZE)) == 0)
				break;
			gfile->getnext = 0;
		}
		ch = *lp++ = gfile->getbuf[gfile->getnext++];
		gfile->getleft--;
		if(ch == '\r') {
			i--;
			lp--;
		}
		if(ch == '\n') {
			i++;
			break;
		}
	}
	if(i<size)
		*lp++ = '\0';
	return i;
}

/* Get a line from the menu file */
/* Return number of lines read from file including comment lines */
static int GetLine(char * buffer, int len, GFILE * gfile)
{
	BOOL status;
	int nLine = 0;

	status = (Gfgets(buffer, len, gfile) != 0);
	nLine++;
	while(status && ( buffer[0] == 0 || buffer[0] == '\n' || buffer[0] == ';' ) ) {
		/* blank line or comment - ignore */
		status = (Gfgets(buffer, len, gfile) != 0);
		nLine++;
	}
	if(lstrlen(buffer)>0)
		buffer[lstrlen(buffer)-1] = '\0';  /* remove trailing \n */

	if(!status)
		nLine = 0;  /* zero lines if file error */

	return nLine;
}

/* Left justify string */
static void LeftJustify(char * d, char * s)
{
	while(*s && (*s==' ' || *s=='\t') )
		s++;    /* skip over space */
	do {
		*d++ = *s;
	} while(*s++);
}

/* Translate string to tokenized macro */
static void TranslateMacro(char * string)
{
	int i, len;
	LPSTR ptr;

	for(i = 0; keyword[i]!=(char*)NULL; i++) {
		if( (ptr = _fstrstr(string, keyword[i])) != NULL) {
			len = lstrlen(keyword[i]);
			*ptr = keyeq[i];
			lstrcpy(ptr+1, ptr+len);
			i--; /* allows for more than one occurrence of keyword */
		}
	}
}

/* Load Macros, and create Menu from Menu file */
void LoadMacros(LPTW lptw)
{
	GFILE * menufile;
	BYTE * macroptr;
	char * buf;
	int nMenuLevel;
	HMENU hMenu[MENUDEPTH+1];
	LPMW lpmw;
	int nLine = 1;
	int nInc;
	HGLOBAL hmacro, hmacrobuf;
	int i;
	RECT rect;
	char * ButtonText[BUTTONMAX];
	int ButtonIcon[BUTTONMAX];
	lpmw = lptw->lpmw;
	/* mark all buffers and menu file as unused */
	buf = (char*)NULL;
	hmacro = 0;
	hmacrobuf = 0;
	lpmw->macro = (BYTE**)NULL;
	lpmw->macrobuf = (BYTE*)NULL;
	lpmw->szPrompt = (char*)NULL;
	lpmw->szAnswer = (char*)NULL;
	menufile = (GFILE*)NULL;

	/* open menu file */
	if((menufile = Gfopen(lpmw->szMenuName, OF_READ)) == (GFILE*)NULL)
		goto errorcleanup;

	/* allocate buffers */
	if((buf = (char *)LocalAllocPtr(LHND, MAXSTR)) == (char*)NULL)
		goto nomemory;
	hmacro = GlobalAlloc(GHND, (NUMMENU)*sizeof(BYTE *));
	if((lpmw->macro = (BYTE**)GlobalLock(hmacro))  == (BYTE**)NULL)
		goto nomemory;
	hmacrobuf = GlobalAlloc(GHND, MACROLEN);
	if((lpmw->macrobuf = (BYTE*)GlobalLock(hmacrobuf)) == (BYTE*)NULL)
		goto nomemory;
	if((lpmw->szPrompt = (char *)LocalAllocPtr(LHND, MAXSTR)) == (char*)NULL)
		goto nomemory;
	if((lpmw->szAnswer = (char *)LocalAllocPtr(LHND, MAXSTR)) == (char*)NULL)
		goto nomemory;

	macroptr = lpmw->macrobuf;
	lpmw->nButton = 0;
	lpmw->nCountMenu = 0;
	lpmw->hMenu = hMenu[0] = CreateMenu();
	nMenuLevel = 0;

	while((nInc = GetLine(buf, MAXSTR, menufile)) != 0) {
		nLine += nInc;
		LeftJustify(buf, buf);
		if(buf[0]=='\0') {
			/* ignore blank lines */
		}
		else if(!lstrcmpi(buf, "[Menu]")) {
			/* new menu */
			if(!(nInc = GetLine(buf, MAXSTR, menufile))) {
				nLine += nInc;
				wsprintf(buf, "Problem on line %d of %s\n", nLine, lpmw->szMenuName);
				MessageBox(lptw->hWndParent, (LPSTR)buf, lptw->Title, MB_ICONEXCLAMATION);
				goto errorcleanup;
			}
			LeftJustify(buf, buf);
			if(nMenuLevel<MENUDEPTH)
				nMenuLevel++;
			else {
				wsprintf(buf, "Menu is too deep at line %d of %s\n", nLine, lpmw->szMenuName);
				MessageBox(lptw->hWndParent, (LPSTR)buf, lptw->Title, MB_ICONEXCLAMATION);
				goto errorcleanup;
			}
			hMenu[nMenuLevel] = CreateMenu();
			AppendMenu(hMenu[nMenuLevel > 0 ? nMenuLevel-1 : 0],
			    MF_STRING | MF_POPUP, (UINT_PTR)hMenu[nMenuLevel], (LPCSTR)buf);
		}
		else if(!lstrcmpi(buf, "[EndMenu]")) {
			if(nMenuLevel > 0)
				nMenuLevel--;  /* back up one menu */
		}
		else if(!lstrcmpi(buf, "[Button]")) {
			/* button macro */
			char * icon;
			if(lpmw->nButton >= BUTTONMAX) {
				wsprintf(buf, "Too many buttons at line %d of %s\n", nLine, lpmw->szMenuName);
				MessageBox(lptw->hWndParent, (LPSTR)buf, lptw->Title, MB_ICONEXCLAMATION);
				goto errorcleanup;
			}
			if(!(nInc = GetLine(buf, MAXSTR, menufile))) {
				nLine += nInc;
				wsprintf(buf, "Problem on line %d of %s\n", nLine, lpmw->szMenuName);
				MessageBox(lptw->hWndParent, (LPSTR)buf, lptw->Title, MB_ICONEXCLAMATION);
				goto errorcleanup;
			}
			LeftJustify(buf, buf);
			if(lstrlen(buf)+1 < MACROLEN - (macroptr-lpmw->macrobuf))
				lstrcpy((char*)macroptr, buf);
			else {
				wsprintf(buf, "Out of space for storing menu macros\n at line %d of %s\n", nLine, lpmw->szMenuName);
				MessageBox(lptw->hWndParent, (LPSTR)buf, lptw->Title, MB_ICONEXCLAMATION);
				goto errorcleanup;
			}
			ButtonText[lpmw->nButton] = (char*)macroptr;
			ButtonIcon[lpmw->nButton] = I_IMAGENONE; /* comctl 5.81, Win 2000 */
			if((icon = strchr((char*)macroptr, ';'))) {
				*icon = NUL;
				ButtonIcon[lpmw->nButton] = atoi(++icon);
			}
			macroptr += lstrlen((char*)macroptr)+1;
			*macroptr = '\0';
			if(!(nInc = GetLine(buf, MAXSTR, menufile))) {
				nLine += nInc;
				wsprintf(buf, "Problem on line %d of %s\n", nLine, lpmw->szMenuName);
				MessageBox(lptw->hWndParent, (LPSTR)buf, lptw->Title, MB_ICONEXCLAMATION);
				goto errorcleanup;
			}
			LeftJustify(buf, buf);
			TranslateMacro(buf);
			if(lstrlen(buf)+1 < MACROLEN - (macroptr - lpmw->macrobuf))
				lstrcpy((char*)macroptr, buf);
			else {
				wsprintf(buf, "Out of space for storing menu macros\n at line %d of %s \n", nLine, lpmw->szMenuName);
				MessageBox(lptw->hWndParent, (LPSTR)buf, lptw->Title, MB_ICONEXCLAMATION);
				goto errorcleanup;
			}
			lpmw->hButtonID[lpmw->nButton] = lpmw->nCountMenu;
			lpmw->macro[lpmw->nCountMenu] = macroptr;
			macroptr += lstrlen((char*)macroptr)+1;
			*macroptr = '\0';
			lpmw->nCountMenu++;
			lpmw->nButton++;
		}
		else {
			/* menu item */
			if(lpmw->nCountMenu>=NUMMENU) {
				wsprintf(buf, "Too many menu items at line %d of %s\n", nLine, lpmw->szMenuName);
				MessageBox(lptw->hWndParent, (LPSTR)buf, lptw->Title, MB_ICONEXCLAMATION);
				goto errorcleanup;
			}
			LeftJustify(buf, buf);
/* HBB 981202: added MF_SEPARATOR to the MF_MENU*BREAK items. This  is meant
 * to maybe avoid a CodeGuard warning about passing last argument zero
 * when item style is not SEPARATOR... Actually, a better solution would
 * have been to combine the '|' divider with the next menu item. */
			if(buf[0]=='-') {
				if(nMenuLevel == 0)
					AppendMenu(hMenu[0], MF_SEPARATOR | MF_MENUBREAK, 0, (LPSTR)NULL);
				else
					AppendMenu(hMenu[nMenuLevel], MF_SEPARATOR, 0, (LPSTR)NULL);
			}
			else if(buf[0]=='|') {
				AppendMenu(hMenu[nMenuLevel], MF_SEPARATOR | MF_MENUBARBREAK, 0, (LPSTR)NULL);
			}
			else {
				AppendMenu(hMenu[nMenuLevel], MF_STRING, lpmw->nCountMenu, (LPSTR)buf);
				if(!(nInc = GetLine(buf, MAXSTR, menufile))) {
					nLine += nInc;
					wsprintf(buf, "Problem on line %d of %s\n", nLine, lpmw->szMenuName);
					MessageBox(lptw->hWndParent, (LPSTR)buf, lptw->Title, MB_ICONEXCLAMATION);
					goto errorcleanup;
				}
				LeftJustify(buf, buf);
				TranslateMacro(buf);
				if(lstrlen(buf)+1 < MACROLEN - (macroptr - lpmw->macrobuf))
					lstrcpy((char*)macroptr, buf);
				else {
					wsprintf(buf, "Out of space for storing menu macros\n at line %d of %s\n", nLine, lpmw->szMenuName);
					MessageBox(lptw->hWndParent, (LPSTR)buf, lptw->Title, MB_ICONEXCLAMATION);
					goto errorcleanup;
				}
				lpmw->macro[lpmw->nCountMenu] = macroptr;
				macroptr += lstrlen((char*)macroptr)+1;
				*macroptr = '\0';
				lpmw->nCountMenu++;
			}
		}
	}

	if( (lpmw->nCountMenu - lpmw->nButton) > 0) {
		/* we have a menu bar so put it on the window */
		SetMenu(lptw->hWndParent, lpmw->hMenu);
		DrawMenuBar(lptw->hWndParent);
	}

	if(!lpmw->nButton)
		goto cleanup;           /* no buttons */

	/* create a toolbar */
	lpmw->hToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
	    WS_CHILD | TBSTYLE_LIST,     // TBSTYLE_WRAPABLE
	    0, 0, 0, 0,
	    lptw->hWndParent, (HMENU)ID_TOOLBAR, lptw->hInstance, NULL);
	if(lpmw->hToolbar == NULL)
		goto cleanup;

	/* set size of toolbar icons */
	/* lparam is (height<<16 + width) / default 16,15 */
	SendMessage(lpmw->hToolbar, TB_SETBITMAPSIZE, (WPARAM)0, (LPARAM)((16<<16) + 16));
	/* load standard toolbar icons: standard, history & view */
	SendMessage(lpmw->hToolbar, TB_LOADIMAGES, (WPARAM)IDB_STD_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);
	SendMessage(lpmw->hToolbar, TB_LOADIMAGES, (WPARAM)IDB_HIST_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);
	SendMessage(lpmw->hToolbar, TB_LOADIMAGES, (WPARAM)IDB_VIEW_SMALL_COLOR, (LPARAM)HINST_COMMCTRL);

	/* create buttons */
	SendMessage(lpmw->hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
	for(i = 0; i < lpmw->nButton; i++) {
		TBBUTTON button;
		ZeroMemory(&button, sizeof(button));
		button.iBitmap = ButtonIcon[i];
		button.idCommand = lpmw->hButtonID[i];
		button.fsState = TBSTATE_ENABLED;
		button.fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT | BTNS_NOPREFIX;
		if(MacroCommand(lptw, lpmw->hButtonID[i]) == OPTIONS)
			button.fsStyle |= BTNS_WHOLEDROPDOWN;
		button.iString = (UINT_PTR)ButtonText[i];
		SendMessage(lpmw->hToolbar, TB_INSERTBUTTON, (WPARAM)i + 1, (LPARAM)&button);
	}

	/* auto-resize and show */
	SendMessage(lpmw->hToolbar, TB_AUTOSIZE, (WPARAM)0, (LPARAM)0);
	ShowWindow(lpmw->hToolbar, true);

	/* move top of client text window down to allow space for toolbar */
	GetClientRect(lpmw->hToolbar, &rect);
	lptw->ButtonHeight = rect.bottom + 1;
	lptw->ButtonHeight++;
	GetClientRect(lptw->hWndParent, &rect);
	SetWindowPos(lptw->hWndText, (HWND)NULL, 0, lptw->ButtonHeight,
	    rect.right, rect.bottom - lptw->ButtonHeight - lptw->StatusHeight,
	    SWP_NOZORDER | SWP_NOACTIVATE);

	goto cleanup;

nomemory:
	MessageBox(lptw->hWndParent, "Out of memory", lptw->Title, MB_ICONEXCLAMATION);
errorcleanup:
	if(hmacro) {
		GlobalUnlock(hmacro);
		GlobalFree(hmacro);
	}
	if(hmacrobuf) {
		GlobalUnlock(hmacrobuf);
		GlobalFree(hmacrobuf);
	}
	if(lpmw->szPrompt != (char*)NULL)
		LocalFreePtr((void NEAR*)lpmw->szPrompt);
	if(lpmw->szAnswer != (char*)NULL)
		LocalFreePtr((void NEAR*)lpmw->szAnswer);

cleanup:
	if(buf != (char*)NULL)
		LocalFreePtr((void NEAR*)buf);
	if(menufile != (GFILE*)NULL)
		Gfclose(menufile);
	return;
}

void CloseMacros(LPTW lptw)
{
	LPMW lpmw = lptw->lpmw;
	HGLOBAL hglobal = (HGLOBAL)GlobalHandle(lpmw->macro);
	if(hglobal) {
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
	}
	hglobal = (HGLOBAL)GlobalHandle(lpmw->macrobuf);
	if(hglobal) {
		GlobalUnlock(hglobal);
		GlobalFree(hglobal);
	}
	if(lpmw->szPrompt != (char*)NULL)
		LocalFreePtr(lpmw->szPrompt);
	if(lpmw->szAnswer != (char*)NULL)
		LocalFreePtr(lpmw->szAnswer);
}

/***********************************************************************/
/* InputBoxDlgProc() -  Message handling routine for Input dialog box         */
/***********************************************************************/

INT_PTR CALLBACK InputBoxDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	LPTW lptw = (LPTW)GetWindowLongPtr(GetParent(hDlg), 0);
	LPMW lpmw = lptw->lpmw;
	switch(message) {
		case WM_INITDIALOG:
		    SetDlgItemText(hDlg, ID_PROMPT, lpmw->szPrompt);
		    return( true);
		case WM_COMMAND:
		    switch(LOWORD(wParam)) {
			    case ID_ANSWER:
				return( true);

			    case IDOK:
				lpmw->nChar = GetDlgItemText(hDlg, ID_ANSWER, lpmw->szAnswer, MAXSTR);
				EndDialog(hDlg, true);
				return( true);

			    case IDCANCEL:
				lpmw->szAnswer[0] = 0;
				EndDialog(hDlg, false);
				return( true);

			    default:
				return( false);
		    }
		default:
		    return( false);
	}
}

