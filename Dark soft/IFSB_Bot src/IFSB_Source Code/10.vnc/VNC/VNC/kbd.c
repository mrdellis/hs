//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VNC project. Version 1.9.17.3
//	
// module: kbd.c
// $Revision: 148 $
// $Date: 2014-01-29 14:13:12 +0400 (Wed, 29 Jan 2014) $
// description: 
//	keyboard events handling

#include "project.h"

#define XK_MISCELLANY
#define XK_LATIN1
#define XK_CURRENCY
#define XK_GREEK
#define XK_TECHNICAL
#define XK_XKB_KEYS

//	[v1.0.2-jp1 fix] IOport's patch (Define XK_KATAKANA)
#define XK_KATAKANA

#include "keysymdef.h"
//	[v1.0.2-jp1 fix] IOport's patch (Include "ime.h" for IME control)
#include <ime.h>
#include <WinUser.h>
#include "wnd.h"

#include "kbd_data.h"

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC     (0)
#endif

DWORD dwIgnoreKeys[]={XK_Num_Lock,XK_Caps_Lock,XK_Scroll_Lock};
UCHAR deadChars[] = {0, 0, 0, 0, 0, 0,0 };

ULONG keysymDead=0;

// keyboard locales
HKL hklLocales[40];
int dwLocalesNum;

//
// translates key sym to unicode char
//
WCHAR KbdKS2Unicode(DWORD dwKeySym)
{
	int minVal=0;
	int maxVal,midVal;
	if (dwKeySym == 0xf3){
		dwKeySym=XK_Cyrillic_u;
	}
	else if (dwKeySym == 0xd3){
		dwKeySym=XK_Cyrillic_U;
	}

	if (((dwKeySym >= 0x0020) && (dwKeySym <= 0x007e)) || 
		((dwKeySym >= 0x00a0) && (dwKeySym <= 0x00ff)) || 
		((dwKeySym & 0xff000000) == 0x01000000))
	{
		return (WCHAR)dwKeySym;
	}

	maxVal=_countof(keySymToUnicodeTable);
	while (maxVal >= minVal)
	{
		WORD wKey;
		midVal=(minVal+maxVal)/2;
		wKey=keySymToUnicodeTable[midVal].keySym;
		if (wKey < dwKeySym){
			minVal=midVal+1;
		}
		else if (wKey > dwKeySym){
			maxVal=midVal-1;
		}
		else{
			return keySymToUnicodeTable[midVal].unicode;
		}
	}

	return 0;
}

BOOL KbdIsItAltTabOnly( PVNC_SESSION pSession, ULONG bVK)
{
	unsigned i;
	BOOL bRet = TRUE;
	UCHAR *KbdState = SS_GET_DATA(pSession)->KbdState;
	if ((!(KbdState[VK_MENU] & 0x80)) || (!(KbdState[VK_TAB] & 0x80)) || (!bVK))
	{
#if 0 //TODO:
		if (lpServer->TaskSwitcherInfo.bTaskSwitcherIsShown)
		{
			DestroyTaskSwitcherWnd(lpServer,bVK != 0);
			lpServer->KbdStateInfo.bSkipEvent=true;
		}
		lpServer->KbdStateInfo.bTabPressed=false;
#endif
		bRet=FALSE;
	}
	else
	{
		BOOL brMenu = FALSE;
		BOOL blMenu = FALSE;

		pSession->Desktop.bTabPressed = ((KbdState[VK_TAB] & 0x80) != 0);
		for ( i=0; i < sizeof(SS_GET_DATA(pSession)->KbdState); i++)
		{
			if ( KbdState[i] & 0x80 )
			{
				if (i == VK_RMENU)
				{
					if (blMenu)
					{
						bRet = FALSE;
						break;
					}
					brMenu=TRUE;
				}
				else if (i == VK_LMENU)
				{
					if (brMenu)
					{
						bRet=FALSE;
						break;
					}
					blMenu=TRUE;
				}

				if ((i != VK_MENU) && (i != VK_RMENU) && (i != VK_LMENU) && (i != VK_TAB))
				{
#if 0 //TODO:
					if (lpServer->TaskSwitcherInfo.bTaskSwitcherIsShown)
						DestroyTaskSwitcherWnd(lpServer,false);
					lpServer->KbdStateInfo.bTabPressed=false;
#endif
					bRet = FALSE;
					break;
				}
			}
		}
	}
	return bRet;
}

BOOL KbdIsItAltShiftOnly(PVNC_SESSION pSession, ULONG bVK )
{
	unsigned i;
	BOOL bRet = TRUE;
	UCHAR *KbdState = SS_GET_DATA(pSession)->KbdState;
	BOOL brMenu = FALSE;
	BOOL blMenu = FALSE,brShift=FALSE,blShift=FALSE;

	if ((!(KbdState[VK_MENU] & 0x80)) || (!(KbdState[VK_SHIFT] & 0x80)) || (!bVK)){
		return FALSE;
	}

	pSession->Desktop.bShiftPressed = ((KbdState[VK_SHIFT] & 0x80) != 0);
	for ( i = 0; i < sizeof(SS_GET_DATA(pSession)->KbdState); i++)
	{
		if ( KbdState[i] & 0x80 )
		{
			if (i == VK_RMENU)
			{
				if (blMenu)
				{
					bRet = FALSE;
					break;
				}
				brMenu=TRUE;
			}
			else if (i == VK_LMENU)
			{
				if (brMenu)
				{
					bRet=FALSE;
					break;
				}
				blMenu = TRUE;
			}
			else if (i == VK_RSHIFT)
			{
				if (blShift)
				{
					bRet = FALSE;
					break;
				}
				brShift = TRUE;
			}
			else if (i == VK_LSHIFT)
			{
				if (brShift)
				{
					bRet = FALSE;
					break;
				}
				blShift = TRUE;
			}

			if ((i != VK_MENU) && (i != VK_RMENU) && (i != VK_LMENU) && (i != VK_SHIFT) && (i != VK_RSHIFT) && (i != VK_LSHIFT))
			{
				bRet = FALSE;
				break;
			}
		}
	}
	return bRet;
}

//
// verifier is char matches the kbd layout
//
BOOL KbdCheckLayout( WORD wVk, WCHAR wcOrig, HKL hLayout )
{
	WCHAR wcTmp;
	BOOL bRet = FALSE;
	BYTE KbdState[256]={0},bFlags=HIBYTE(wVk),bVk=LOBYTE(wVk);

	if (bFlags & 1){
		KbdState[VK_RSHIFT]=KbdState[VK_SHIFT]=0x80;
	}

	if (bFlags & 2){
		KbdState[VK_RCONTROL]=KbdState[VK_CONTROL]=0x80;
	}

	if (bFlags & 4){
		KbdState[VK_RMENU]=KbdState[VK_MENU]=0x80;
	}

	KbdState[bVk]=0x80;

	ToUnicodeEx(bVk,MapVirtualKeyEx(bVk,MAPVK_VK_TO_VSC,hLayout),KbdState,&wcTmp,1,0,hLayout);
	if (wcOrig == wcTmp){
		bRet = TRUE;
	}
	return bRet;
}


//
// returns locale for symbol
//
HKL KbdGetLocaleByChar(HKL hKL, WCHAR wcChar, WORD *lpVK)
{
	HKL hLocale=0;
	BOOL bFound=FALSE;
	SHORT wVk;
	int i,j;

	// char current locale first
	if ( hKL )
	{
		wVk = VkKeyScanExW ( wcChar, hKL );
		if (wVk != -1){
			if ( lpVK ){
				*lpVK=wVk;
			}
			return hKL;
		}
	}

	for ( i = 0; i < dwLocalesNum; i++)
	{
		wVk = VkKeyScanExW ( wcChar, hklLocales[i] );
		if (wVk != -1)
		{
			for ( j = 0; j < dwLocalesNum; j++)
			{
				if (KbdCheckLayout(wVk,wcChar,hklLocales[j]))
				{
					bFound=TRUE;
					if ( lpVK ){
						*lpVK=wVk;
					}
					hLocale=hklLocales[j];
					break;
				}
			}
			if (bFound)
				break;
		}
	}
	return hLocale;
}

//
// update desktop input state: kbd and mouse
//
WORD KbdUpdateInputState(PVNC_SESSION pVncSession,ULONG bVK,BOOL bDown)
{
	PVNC_SHARED_DATA SharedData = SS_GET_DATA(pVncSession);
	WORD wMouseState=0;

	if (bVK && bVK < 256 )
	{
		SharedData->KbdState[bVK]=(bDown) ? 0x80:0;
		switch (bVK)
		{
		case VK_LSHIFT:
		case VK_RSHIFT:
			{
				SharedData->KbdState[VK_SHIFT]=
					((SharedData->KbdState[VK_LSHIFT] & 0x80) || (SharedData->KbdState[VK_RSHIFT] & 0x80)) ? 0x80:0;
				break;
			}

		case VK_LCONTROL:
		case VK_RCONTROL:
			{
				SharedData->KbdState[VK_CONTROL]=
					((SharedData->KbdState[VK_LCONTROL] & 0x80) || (SharedData->KbdState[VK_RCONTROL] & 0x80)) ? 0x80:0;
				break;
			}

		case VK_LMENU:
		case VK_RMENU:
			{
				SharedData->KbdState[VK_MENU]=
					((SharedData->KbdState[VK_LMENU] & 0x80) || (SharedData->KbdState[VK_RMENU] & 0x80)) ? 0x80:0;
				break;
			}
		}
	}

	if ( SharedData->KbdState[VK_LBUTTON] & 0x80 )
		wMouseState|=MK_LBUTTON;
	if ( SharedData->KbdState[VK_RBUTTON] & 0x80 )
		wMouseState|=MK_RBUTTON;
	if ( SharedData->KbdState[VK_MBUTTON] & 0x80 )
		wMouseState|=MK_MBUTTON;
	if ( SharedData->KbdState[VK_SHIFT] & 0x80 )
		wMouseState|=MK_SHIFT;
	if ( SharedData->KbdState[VK_CONTROL] & 0x80 )
		wMouseState|=MK_CONTROL;

	return wMouseState;
}

BOOL KbdCheckTranslateMessage(PVNC_SESSION pSession, HWND hWnd)
{
	HWND Result;
	Result = 
		(HWND)
			SendMessage(
				hWnd,
				SS_GET_DATA(pSession)->dwVNCMessage,
				VMW_ISTRANSMSGUSED,
				0
				);
	DbgPrint("hWnd=%08X\n",hWnd);
	return (Result == hWnd);
}

//
// processes input keyboard event
//
VOID 
	KbdProcessKeyEvent(
		PVNC_SESSION pVncSession, 
		HWND hWnd, 
		ULONG keysym, 
		BOOL down, 
		BOOL jap
		)
{
	unsigned i;
	ULONG vkCode = 0;
	WCHAR wcChar = 0; // keysym->wchar
	WORD wVk = 0;
	HKL hKL = NULL; // locale for char
	HKL hCurLay = NULL;
	USHORT bScanCode = 0;
	BOOL bIsSysKey;

	UINT uMsg = 0;
	LPARAM lParam  = down ? 0:((1<<31)|(1<<30));
	WPARAM wParam = 0;

	BOOL bAltTabOnly;
	BOOL bConsoleWnd = IsConsole( hWnd );

	// check if ignore this key
	for ( i=0; i < _countof(dwIgnoreKeys); i++)
	{
		if (dwIgnoreKeys[i] == keysym)
			return;
	}

	if ((keysym >= 32 && keysym <= 126) ||
		(keysym >= 160 && keysym <= 255))
	{
		// ordinary Latin-1 character
		vkCode = 0;
	}else{

		for ( i = 0; i < _countof(keymap); i++)
		{
			if ( keymap[i].keysym == keysym )
			{
				vkCode = keymap[i].vk;
				if (keymap[i].extended){
					lParam|=(1<<24);
				}
				break;
			}
		}
	}

	// translate keysym to unicode char
	wcChar = KbdKS2Unicode(keysym);

	// current locale
	hCurLay = GetKeyboardLayout(GetWindowThreadProcessId(hWnd,0));

	//get locale for char
	hKL = KbdGetLocaleByChar ( hCurLay, wcChar, &wVk );

	if ( (!vkCode) && (KbdCheckTranslateMessage(pVncSession,hWnd))){
		if ( hKL ){
			vkCode = LOBYTE(wVk);
		}else{
			vkCode = LOBYTE(VkKeyScanEx((CHAR)wcChar,hKL));
		}
	}

	if ( vkCode )
	{
		// update keydown state before event sending
		if ( down ){
			KbdUpdateInputState ( pVncSession, vkCode, down );
		}

		bScanCode=MapVirtualKey( vkCode, MAPVK_VK_TO_VSC ) & 0xFF;
		bIsSysKey=((SS_GET_DATA(pVncSession)->KbdState[VK_MENU] & 0x80) && (!(SS_GET_DATA(pVncSession)->KbdState[VK_CONTROL] & 0x80)));

		if ( down )
		{
			pVncSession->Desktop.bVKDown = TRUE;
			uMsg=(bIsSysKey) ? WM_SYSKEYDOWN:WM_KEYDOWN;
		}
		else
		{
			pVncSession->Desktop.bVKDown = FALSE;
			uMsg=(bIsSysKey) ? WM_SYSKEYUP:WM_KEYUP;
		}

		// update keyup state after event sending
		if ( !down ){
			KbdUpdateInputState ( pVncSession, vkCode, down );
		}

		if (SS_GET_DATA(pVncSession)->KbdState[VK_MENU] & 0x80){
			lParam|=(1<<29);
		}
		// transform lmenu and rmenu to menu
		switch ( vkCode )
		{
		case VK_LMENU:
		case VK_RMENU:
			vkCode = VK_MENU;
			break;
		case VK_LSHIFT:
		case VK_RSHIFT:
			vkCode = VK_SHIFT;
			break;
		}
		wParam = vkCode;
	}
	else
	{
		if (((!pVncSession->Desktop.bVKDown) && 
			(!pVncSession->Desktop.bKeyDown)) || 
				((pVncSession->Desktop.bVKDown) && 
				(SS_GET_DATA(pVncSession)->KbdState[VK_SHIFT] & 0x80) && 
				(!SS_GET_DATA(pVncSession)->KbdState[VK_MENU]) && 
				(!SS_GET_DATA(pVncSession)->KbdState[VK_CONTROL])))
		{
			if ( !down || !wcChar )
			{
				return;
			}

			if (SS_GET_DATA(pVncSession)->KbdState[VK_MENU] & 0x80){
				lParam|=(1<<29);
			}
			wParam = wcChar;
			bScanCode=MapVirtualKey( wcChar, MAPVK_VK_TO_VSC ) & 0xFF;
			bIsSysKey=((SS_GET_DATA(pVncSession)->KbdState[VK_MENU] & 0x80) && 
				(!(SS_GET_DATA(pVncSession)->KbdState[VK_CONTROL] & 0x80)));

			if ( !bConsoleWnd ){
				uMsg = bIsSysKey ? WM_SYSCHAR : WM_CHAR;
			}else{
				if ( down )
				{
					pVncSession->Desktop.bVKDown = TRUE;
					uMsg=(bIsSysKey) ? WM_SYSKEYDOWN:WM_KEYDOWN;
				}
				else
				{
					pVncSession->Desktop.bVKDown = FALSE;
					uMsg=(bIsSysKey) ? WM_SYSKEYUP:WM_KEYUP;
				}
				if (SS_GET_DATA(pVncSession)->KbdState[VK_MENU] & 0x80){
					lParam|=(1<<29);
				}
				wParam = vkCode;
			}
		}
		else
		{
			wParam=(WPARAM)wVk;
			bScanCode=MapVirtualKey( (UINT)wParam, MAPVK_VK_TO_VSC ) & 0xFF;

			bIsSysKey=((SS_GET_DATA(pVncSession)->KbdState[VK_MENU] & 0x80) && 
						(!(SS_GET_DATA(pVncSession)->KbdState[VK_CONTROL] & 0x80)));
			if ( down )
			{
				pVncSession->Desktop.bKeyDown = TRUE;
				uMsg=(bIsSysKey) ? WM_SYSKEYDOWN:WM_KEYDOWN;
			}
			else
			{
				pVncSession->Desktop.bKeyDown = FALSE;
				uMsg=(bIsSysKey) ? WM_SYSKEYUP:WM_KEYUP;
			}
		}
	}

	if (SS_GET_DATA(pVncSession)->bAutodectLayout)
	{
		if (((wVk=VkKeyScanExW(wcChar,hCurLay)) == -1) || (!KbdCheckLayout(wVk,wcChar,hCurLay)))
		{
			if (hKL){
				DWORD_PTR Result;
				SendMessageTimeout(
					hWnd,
					SS_GET_DATA(pVncSession)->dwVNCMessage,
					VMW_CHANGELAYOUT,
					(LPARAM)hKL,
					SMTO_ABORTIFHUNG | SMTO_NORMAL,
					WND_MSG_TIMEOUT,
					&Result
					);
			}
		}
	}

	bAltTabOnly = KbdIsItAltTabOnly(pVncSession,vkCode );

	if ( bAltTabOnly )
	{
		if ( pVncSession->Desktop.bTabPressed )
		{
#if 0 //TODO:
			if (!lpServer->TaskSwitcherInfo.bTaskSwitcherIsShown)
			{
				ShowTaskSwitcherWnd(lpServer);
				lpServer->TaskSwitcherInfo.bTaskSwitcherIsShown=true;
			}
			else
				NextTask(lpServer);
#endif
		}
	}
	else
	{
		BOOL bAltShiftOnly = FALSE;
		if (!pVncSession->Desktop.bSkipEvent)
		{
			bAltShiftOnly = KbdIsItAltShiftOnly(pVncSession,vkCode);
		}

		if ( !pVncSession->Desktop.bSkipEvent )
		{
			do
			{
				DWORD_PTR Result;
				if ( bAltShiftOnly )
				{
					DWORD dwLastShiftInputTime=GetTickCount();
					if ( dwLastShiftInputTime - pVncSession->Desktop.dwLastShiftInputTime < 500 )
						break;
					pVncSession->Desktop.dwLastShiftInputTime=dwLastShiftInputTime;
					if ((KbdCheckTranslateMessage(pVncSession,hWnd)) && (pVncSession->Desktop.bShiftPressed)){
						//TEST
						//SendMessageTimeout(
						//	hWnd,
						//	SS_GET_DATA(pVncSession)->dwVNCMessage,
						//	VMW_CHANGELAYOUT,
						//	(LPARAM)HKL_NEXT,
						//	SMTO_ABORTIFHUNG | SMTO_NORMAL,
						//	WND_MSG_TIMEOUT,
						//	&Result
						//	);
					}
				}
				SendMessageTimeout(
					hWnd,
					SS_GET_DATA(pVncSession)->dwVNCMessage,
					VMW_UPDATE_KEYSTATE,0,
					SMTO_ABORTIFHUNG | SMTO_NORMAL,
					WND_MSG_TIMEOUT,
					&Result
					);

				if ( uMsg == WM_KEYUP ){
					DbgPrint("WM_KEYUP hWnd=%p vk=%i\n",hWnd, wParam);
				}else if ( uMsg == WM_KEYDOWN ){
					DbgPrint("WM_KEYDOWN hWnd=%p vk=%i\n",hWnd, wParam);
				}else if ( uMsg == WM_SYSKEYUP ){
					DbgPrint("WM_SYSKEYUP hWnd=%p vk=%i\n",hWnd, wParam);
				}else if ( uMsg == WM_SYSKEYDOWN ){
					DbgPrint("WM_SYSKEYDOWN hWnd=%p vk=%i\n",hWnd, wParam);
				}

				PostMessage(hWnd,uMsg,wParam,lParam | bScanCode << 16 | 1 );
			}
			while ( FALSE );
		}
		pVncSession->Desktop.bSkipEvent = FALSE;
	}
	return;
}
//
// VNC keyboard event handler
//
VOID KbdOnKeyEvent(PVNC_SESSION pVncSession, ULONG keysym, BOOL down)
{
	HWND hPopupWnd,hFocusWnd;
	HWND hWnd = SS_GET_FOREGROUND_WND(pVncSession);

	if ((!hWnd) || (!IsWindowEx(&pVncSession->Desktop,hWnd)) || (!IsWindowVisibleEx(hWnd)) || (IsIconic(hWnd))){
		hWnd=GetLastActivePopup(pVncSession->Desktop.hDeskWnd);
	}
	else
	{
		hPopupWnd=GetTopPopupWindow(&pVncSession->Desktop,hWnd);
		if (hPopupWnd)
		{
			hWnd=hPopupWnd;
			SetForegroundWnd(pVncSession,hWnd);
		}
		
		hFocusWnd=GetWindowFocus(hWnd);
		if (hFocusWnd){
			hWnd = hFocusWnd;
		}
	}
	KbdProcessKeyEvent(pVncSession, hWnd, keysym, down, FALSE);
}

DWORD KbdGetLocalesNum()
{
	return dwLocalesNum;
}

HKL* KbdGetLocales()
{
	return hklLocales;
}

//
// initializes global state
//
VOID KeyBoardInit()
{
	// Find dead characters for the current keyboard layout
	// XXX how could we handle the keyboard layout changing?
	int i,j;
	UCHAR keystate[256];
	memset(keystate, 0, 256);

	for ( i = 0, j = 0; j < sizeof(latin1DeadChars); j++) {
		SHORT s = VkKeyScan(latin1DeadChars[j]);
		if (s != -1) 
		{
			UCHAR vkCode = LOBYTE(s);
			UCHAR modifierState = HIBYTE(s);
			UCHAR chars[2];
			int nchars;
			keystate[VK_SHIFT] = (modifierState & 1) ? 0x80 : 0;
			keystate[VK_CONTROL] = (modifierState & 2) ? 0x80 : 0;
			keystate[VK_MENU] = (modifierState & 4) ? 0x80 : 0;

			nchars = ToAscii(vkCode, 0, keystate, (WORD*)&chars, 0);
			if (nchars < 0) {
				deadChars[i++] = latin1DeadChars[j];
				ToAscii(vkCode, 0, keystate, (WORD*)&chars, 0);
			}
		}
	}

	dwLocalesNum=GetKeyboardLayoutList(_countof(hklLocales),hklLocales);
}
