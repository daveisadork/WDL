/* Cockos SWELL (Simple/Small Win32 Emulation Layer for Losers (who use OS X))
   Copyright (C) 2006-2007, Cockos, Inc.

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
       claim that you wrote the original software. If you use this software
       in a product, an acknowledgment in the product documentation would be
       appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
       misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.


    This file provides basic APIs for browsing for files, directories, and messageboxes.

    These APIs don't all match the Windows equivelents, but are close enough to make it not too much trouble.

  */


#ifndef SWELL_PROVIDED_BY_APP

#include "swell.h"
#include "swell-internal.h"
#include "swell-dlggen.h"
#include <gtk/gtk.h>

static const char *BFSF_Templ_dlgid;
static DLGPROC BFSF_Templ_dlgproc;
static struct SWELL_DialogResourceIndex *BFSF_Templ_reshead;
void BrowseFile_SetTemplate(const char *dlgid, DLGPROC dlgProc, struct SWELL_DialogResourceIndex *reshead)
{
  BFSF_Templ_reshead=reshead;
  BFSF_Templ_dlgid=dlgid;
  BFSF_Templ_dlgproc=dlgProc;
}

// return true
bool BrowseForSaveFile(const char *text, const char *initialdir, const char *initialfile, const char *extlist,
                       char *fn, int fnsize)
{
  printf("browse for save file - text: %s, initialdir: %s, initialfile: %s
         bool ret = false;
         GtkWidget *dialog;
         dialog = gtk_file_chooser_dialog_new (text,
         NULL,
         GTK_FILE_CHOOSER_ACTION_SAVE,
         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
         NULL);
         if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
       {
         fn = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
         fnsize = strlen(fn);
         ret = true;
       }
         gtk_widget_destroy (dialog);
         /*int argc = 0;
         char** argv = NULL;
         QApplication myApp(argc, argv);
         QString fileName = QFileDialog::getSaveFileName (0, QString(text), QStr
         if (fileName == "") {
         return false;
       } else {
         strncpy(fn, fileName.toUtf8(), fnsize);
         return true;
       }*/
         return ret;
       }

         bool BrowseForDirectory(const char *text, const char *initialdir, char *fn, int fnsize)
       {
         return false;
       }


         char *BrowseForFiles(const char *text, const char *initialdir,
         const char *initialfile, bool allowmul, const char *extlist)
       {
         return NULL;
       }


         static LRESULT WINAPI swellMessageBoxProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
       {
         switch (uMsg)
       {
         case WM_CREATE:
         if (lParam)  // swell-specific
       {
         SetWindowLong(hwnd,GWL_WNDPROC,(LPARAM)SwellDialogDefaultWindowProc);
         SetWindowLong(hwnd,DWL_DLGPROC,(LPARAM)swellMessageBoxProc);
         void **parms = (void **)lParam;
         if (parms[1]) SetWindowText(hwnd,(const char*)parms[1]);


         int nbuttons=1;
         const char *buttons[3] = { "OK", "", "" };
         int button_ids[3] = {IDOK,0,0};
         int mode =  ((int)(INT_PTR)parms[2]);
         if (mode == MB_RETRYCANCEL) { buttons[0]="Retry"; button_ids[0]=IDRETRY;  }
         if (mode == MB_YESNO || mode == MB_YESNOCANCEL) { buttons[0]="Yes"; button_ids[0] = IDYES;  buttons[nbuttons] = "No"; button_ids[nbuttons] = IDNO; nbuttons++; }
         if (mode == MB_OKCANCEL || mode == MB_YESNOCANCEL || mode == MB_RETRYCANCEL) { buttons[nbuttons] = "Cancel"; button_ids[nbuttons] = IDCANCEL; nbuttons++; }

         SWELL_MakeSetCurParms(1,1,0,0,hwnd,false,false);
         RECT labsize = {0,0,300,80};
         HWND lab = SWELL_MakeLabel(-1,parms[0] ? (const char *)parms[0] : "", 0, 0,0,10,10,0); //we'll resize this manually
         if (lab && parms[0])
       {
         HDC dc=GetDC(lab);
         DrawText(dc,(const char *)parms[0],-1,&labsize,DT_CALCRECT);// if dc isnt valid yet, try anyway
         if (dc) ReleaseDC(lab,dc);
       }

         int buttonh=12;
         int buttonw = 35, buttonspace=5;
         int buttontotalw = nbuttons * buttonw + (nbuttons-1)*buttonspace;
         if (labsize.right < buttontotalw) labsize.right = buttontotalw;
         if (labsize.bottom < 40) labsize.bottom = 40;

         int x,xpos = labsize.right/2 - buttontotalw/2;
         for (x = 0; x < nbuttons; x ++)
       {
         SWELL_MakeButton(0,buttons[x],button_ids[x],xpos,labsize.bottom+4,buttonw,buttonh,0);
         xpos += buttonw + buttonspace;
       }

         SWELL_MakeSetCurParms(1,1,0,0,NULL,false,false);
         if (lab) SetWindowPos(lab,NULL,0,0,labsize.right+3,labsize.bottom,SWP_NOACTIVATE|SWP_NOZORDER);
         SetWindowPos(hwnd,NULL,0,0,labsize.right+10,labsize.bottom+buttonh+4,SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOMOVE);
       }
         break;
         case WM_COMMAND:
         if (LOWORD(wParam) && HIWORD(wParam) == BN_CLICKED ) EndDialog(hwnd,LOWORD(wParam));
         break;
         case WM_CLOSE:
         if (GetDlgItem(hwnd,IDCANCEL)) EndDialog(hwnd,IDCANCEL);
         else if (GetDlgItem(hwnd,IDNO)) EndDialog(hwnd,IDNO);
         else if (GetDlgItem(hwnd,IDYES)) EndDialog(hwnd,IDYES);
         else EndDialog(hwnd,IDOK);
         break;
       }
         return 0;
       }

         int MessageBox(HWND hwndParent, const char *text, const char *caption, int type)
       {
       printf("MessageBox: %s %s\n",text,caption);
         const void *parms[3]= {text,caption,(void*)(INT_PTR)type} ;
         return DialogBoxParam(NULL,NULL,NULL,swellMessageBoxProc,(LPARAM)parms);

#if 0
         int ret=0;

         if (type == MB_OK)
       {
         // todo
         ret=IDOK;
       }
         else if (type == MB_OKCANCEL)
       {
         ret = 1; // todo
         if (ret) ret=IDOK;
         else ret=IDCANCEL;
       }
         else if (type == MB_YESNO)
       {
         ret = 1 ; // todo
         if (ret) ret=IDYES;
         else ret=IDNO;
       }
         else if (type == MB_RETRYCANCEL)
       {
         ret = 1; // todo

         if (ret) ret=IDRETRY;
         else ret=IDCANCEL;
       }
         else if (type == MB_YESNOCANCEL)
       {
         ret = 1; // todo

         if (ret == 1) ret=IDYES;
         else if (ret==-1) ret=IDNO;
         else ret=IDCANCEL;
       }

         return ret;
#endif
       }

#endif
