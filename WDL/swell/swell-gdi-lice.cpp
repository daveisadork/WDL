
/* Cockos SWELL (Simple/Small Win32 Emulation Layer for Linux)
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


    This file provides basic win32 GDI-->lice translation.

*/

#ifdef SWELL_LICE_GDI
#ifndef SWELL_PROVIDED_BY_APP

#include "swell.h"
#include "swell-internal.h"

#include "../mutex.h"
#include "../ptrlist.h"

#include "swell-gdi-internalpool.h"


HDC SWELL_CreateMemContext(HDC hdc, int w, int h)
{
  LICE_MemBitmap * bm = new LICE_MemBitmap(w,h);
  if (!bm) return 0;

  HDC__ *ctx=SWELL_GDP_CTX_NEW();
  ctx->surface = bm;
  ctx->surface_offs.x=0;
  ctx->surface_offs.y=0;
  ctx->dirty_rect_valid=false;

  SetTextColor(ctx,0);
  return ctx;
}


void SWELL_DeleteGfxContext(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (HDC_VALID(ct))
  {
    delete ct->surface;
    ct->surface=0;
    SWELL_GDP_CTX_DELETE(ct);
  }
}

HPEN CreatePen(int attr, int wid, int col)
{
  return CreatePenAlpha(attr,wid,col,1.0f);
}

HBRUSH CreateSolidBrush(int col)
{
  return CreateSolidBrushAlpha(col,1.0f);
}

HPEN CreatePenAlpha(int attr, int wid, int col, float alpha)
{
  HGDIOBJ__ *pen=GDP_OBJECT_NEW();
  pen->type=TYPE_PEN;
  pen->wid=wid<0?0:wid;
  pen->color=LICE_RGBA_FROMNATIVE(col);
  return pen;
}
HBRUSH  CreateSolidBrushAlpha(int col, float alpha)
{
  HGDIOBJ__ *brush=GDP_OBJECT_NEW();
  brush->type=TYPE_BRUSH;
  brush->color=LICE_RGBA_FROMNATIVE(col);
  brush->wid=0;
  return brush;
}

void SetBkColor(HDC ctx, int col)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return;
  ct->curbkcol=LICE_RGBA_FROMNATIVE(col,255);
}

void SetBkMode(HDC ctx, int col)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return;
  ct->curbkmode=col;
}

HGDIOBJ GetStockObject(int wh)
{
  switch (wh)
  {
  case NULL_BRUSH:
  {
    static HGDIOBJ__ br= {0,};
    br.type=TYPE_BRUSH;
    br.wid=-1;
    return &br;
  }
  case NULL_PEN:
  {
    static HGDIOBJ__ pen= {0,};
    pen.type=TYPE_PEN;
    pen.wid=-1;
    return &pen;
  }
  }
  return 0;
}


#define FONTSCALE 0.9
HFONT CreateFont(int lfHeight, int lfWidth, int lfEscapement, int lfOrientation, int lfWeight, char lfItalic,
                 char lfUnderline, char lfStrikeOut, char lfCharSet, char lfOutPrecision, char lfClipPrecision,
                 char lfQuality, char lfPitchAndFamily, const char *lfFaceName)
{
  HGDIOBJ__ *font=GDP_OBJECT_NEW();
  font->type=TYPE_FONT;
  float fontwid=lfHeight;


  if (!fontwid) fontwid=lfWidth;
  if (fontwid<0)fontwid=-fontwid;

  if (fontwid < 2 || fontwid > 8192) fontwid=10;

  return font;
}


HFONT CreateFontIndirect(LOGFONT *lf)
{
  return CreateFont(lf->lfHeight, lf->lfWidth,lf->lfEscapement, lf->lfOrientation, lf->lfWeight, lf->lfItalic,
                    lf->lfUnderline, lf->lfStrikeOut, lf->lfCharSet, lf->lfOutPrecision,lf->lfClipPrecision,
                    lf->lfQuality, lf->lfPitchAndFamily, lf->lfFaceName);
}

void DeleteObject(HGDIOBJ pen)
{
  if (HGDIOBJ_VALID(pen))
  {
    HGDIOBJ__ *p=(HGDIOBJ__ *)pen;
    if (p->type == TYPE_PEN || p->type == TYPE_BRUSH || p->type == TYPE_FONT || p->type == TYPE_BITMAP)
    {
      if (p->type == TYPE_PEN || p->type == TYPE_BRUSH)
        if (p->wid<0) return;

      GDP_OBJECT_DELETE(p);
    }
    else free(p);
  }
}


HGDIOBJ SelectObject(HDC ctx, HGDIOBJ pen)
{
  HDC__ *c=(HDC__ *)ctx;
  HGDIOBJ__ *p= pen;
  HGDIOBJ__ **mod=0;
  if (!HDC_VALID(c)||!p) return 0;

  if (p == (HGDIOBJ__ *)TYPE_PEN) mod=&c->curpen;
  else if (p == (HGDIOBJ__ *)TYPE_BRUSH) mod=&c->curbrush;
  else if (p == (HGDIOBJ__ *)TYPE_FONT) mod=&c->curfont;

  if (mod)
  {
    HGDIOBJ__ *np=*mod;
    *mod=0;
    return np?np:p;
  }

  if (!HGDIOBJ_VALID(p)) return 0
                                  ;
  if (p->type == TYPE_PEN) mod=&c->curpen;
  else if (p->type == TYPE_BRUSH) mod=&c->curbrush;
  else if (p->type == TYPE_FONT) mod=&c->curfont;
  else return 0;

  HGDIOBJ__ *op=*mod;
  if (!op) op=(HGDIOBJ__ *)p->type;
  if (op != p)
  {
    *mod=p;

    if (p->type == TYPE_FONT)
    {
//      CGContextSelectFont(c->ctx,p->fontface,(float)p->wid,kCGEncodingMacRoman);
    }
  }
  return op;
}


static void swell_DirtyContext(HDC__ *out, int x1, int y1, int x2, int y2)
{
  if (x2<x1) {
    int t=x1;
    x1=x2;
    x2=t;
  }
  if (y2<y1) {
    int t=y1;
    y1=y2;
    y2=t;
  }
  x1+=out->surface_offs.x;
  x2+=out->surface_offs.x;
  y1+=out->surface_offs.y;
  y2+=out->surface_offs.y;

  if (!out->dirty_rect_valid)
  {
    out->dirty_rect_valid=true;
    out->dirty_rect.left = x1;
    out->dirty_rect.top = y1;
    out->dirty_rect.right = x2;
    out->dirty_rect.bottom = y2;
  }
  else
  {
    if (out->dirty_rect.left > x1) out->dirty_rect.left=x1;
    if (out->dirty_rect.top > y1) out->dirty_rect.top = y1;
    if (out->dirty_rect.right < x2) out->dirty_rect.right = x2;
    if (out->dirty_rect.bottom < y2) out->dirty_rect.bottom = y2;
  }
}


void SWELL_FillRect(HDC ctx, RECT *r, HBRUSH br)
{
  HDC__ *c=(HDC__ *)ctx;
  HGDIOBJ__ *b=(HGDIOBJ__ *) br;
  if (!HDC_VALID(c) || !HGDIOBJ_VALID(b,TYPE_BRUSH)) return;
  if (!c->surface) return;

  if (b->wid<0) return;
  LICE_FillRect(c->surface,r->left,r->top,r->right-r->left,r->bottom-r->top,b->color,1.0f,LICE_BLIT_MODE_COPY);

}

void RoundRect(HDC ctx, int x, int y, int x2, int y2, int xrnd, int yrnd)
{
  xrnd/=3;
  yrnd/=3;
  POINT pts[10]= { // todo: curves between edges
    {x,y+yrnd},
    {x+xrnd,y},
    {x2-xrnd,y},
    {x2,y+yrnd},
    {x2,y2-yrnd},
    {x2-xrnd,y2},
    {x+xrnd,y2},
    {x,y2-yrnd},
    {x,y+yrnd},
    {x+xrnd,y},
  };

  WDL_GDP_Polygon(ctx,pts,sizeof(pts)/sizeof(pts[0]));
}

void Ellipse(HDC ctx, int l, int t, int r, int b)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)) return;

  //CGRect rect=CGRectMake(l,t,r-l,b-t);

  if (HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH) && c->curbrush->wid >=0)
  {
  }
  if (HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid >= 0)
  {
  }
}

void Rectangle(HDC ctx, int l, int t, int r, int b)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)) return;

  //CGRect rect=CGRectMake(l,t,r-l,b-t);

  if (HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH) && c->curbrush->wid >= 0)
  {
    LICE_FillRect(c->surface,l,t,r-l,b-t,c->curbrush->color,1.0f,LICE_BLIT_MODE_COPY);
  }
  if (HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid >= 0)
  {
    LICE_DrawRect(c->surface,l,t,r-l,b-t,c->curpen->color,1.0f,LICE_BLIT_MODE_COPY);
  }
}

void Polygon(HDC ctx, POINT *pts, int npts)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)) return;
  if (((!HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH)||c->curbrush->wid<0) &&
       (!HGDIOBJ_VALID(c->curpen,TYPE_PEN) ||c->curpen->wid<0)) || npts<2) return;

//  CGContextBeginPath(c->ctx);
// CGContextMoveToPoint(c->ctx,(float)pts[0].x,(float)pts[0].y);
  int x;
  for (x = 1; x < npts; x ++)
  {
    //  CGContextAddLineToPoint(c->ctx,(float)pts[x].x,(float)pts[x].y);
  }
  if (HGDIOBJ_VALID(c->curbrush,TYPE_BRUSH) && c->curbrush->wid >= 0)
  {
    // CGContextSetFillColorWithColor(c->ctx,c->curbrush->color);
  }
  if (HGDIOBJ_VALID(c->curpen,TYPE_PEN) && c->curpen->wid>=0)
  {
//    CGContextSetLineWidth(c->ctx,(float)max(c->curpen->wid,1));
//   CGContextSetStrokeColorWithColor(c->ctx,c->curpen->color);
  }
//  CGContextDrawPath(c->ctx,c->curpen && c->curpen->wid>=0 && c->curbrush && c->curbrush->wid>=0 ?  kCGPathFillStroke : c->curpen && c->curpen->wid>=0 ? kCGPathStroke : kCGPathFill);
}

void MoveToEx(HDC ctx, int x, int y, POINT *op)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)) return;
  if (op)
  {
    op->x = (int) (c->lastpos_x);
    op->y = (int) (c->lastpos_y);
  }
  c->lastpos_x=(float)x;
  c->lastpos_y=(float)y;
}

void PolyBezierTo(HDC ctx, POINT *pts, int np)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)||!HGDIOBJ_VALID(c->curpen,TYPE_PEN)||c->curpen->wid<0||np<3) return;

//  CGContextSetLineWidth(c->ctx,(float)max(c->curpen->wid,1));
//  CGContextSetStrokeColorWithColor(c->ctx,c->curpen->color);

//  CGContextBeginPath(c->ctx);
//  CGContextMoveToPoint(c->ctx,c->lastpos_x,c->lastpos_y);
  int x;
  float xp,yp;
  for (x = 0; x < np-2; x += 3)
  {
    /*    CGContextAddCurveToPoint(c->ctx,
          (float)pts[x].x,(float)pts[x].y,
          (float)pts[x+1].x,(float)pts[x+1].y,
    */
    xp=(float)pts[x+2].x;
    yp=(float)pts[x+2].y;
  }
  c->lastpos_x=(float)xp;
  c->lastpos_y=(float)yp;
//  CGContextStrokePath(c->ctx);
}


void SWELL_LineTo(HDC ctx, int x, int y)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)||!HGDIOBJ_VALID(c->curpen,TYPE_PEN)||c->curpen->wid<0) return;

//  CGContextSetLineWidth(c->ctx,(float)max(c->curpen->wid,1));
//  CGContextSetStrokeColorWithColor(c->ctx,c->curpen->color);

//  CGContextBeginPath(c->ctx);
//  CGContextMoveToPoint(c->ctx,c->lastpos_x,c->lastpos_y);
  float fx=(float)x,fy=(float)y;

  int dx=c->surface_offs.x;
  int dy=c->surface_offs.y;
  LICE_Line(c->surface,x+dx,y+dy,(int)c->lastpos_x+dx,(int)c->lastpos_y+dy,c->curpen->color,1.0f,LICE_BLIT_MODE_COPY,false);

//  CGContextAddLineToPoint(c->ctx,fx,fy);
  c->lastpos_x=fx;
  c->lastpos_y=fy;
//  CGContextStrokePath(c->ctx);
}

void PolyPolyline(HDC ctx, POINT *pts, DWORD *cnts, int nseg)
{
  HDC__ *c=(HDC__ *)ctx;
  if (!HDC_VALID(c)||!HGDIOBJ_VALID(c->curpen,TYPE_PEN)||c->curpen->wid<0||nseg<1) return;

//  CGContextSetLineWidth(c->ctx,(float)max(c->curpen->wid,1));
//  CGContextSetStrokeColorWithColor(c->ctx,c->curpen->color);

//  CGContextBeginPath(c->ctx);

  while (nseg-->0)
  {
    DWORD cnt=*cnts++;
    if (!cnt) continue;
    if (!--cnt) {
      pts++;
      continue;
    }

//   CGContextMoveToPoint(c->ctx,(float)pts->x,(float)pts->y);
    pts++;

    while (cnt--)
    {
//      CGContextAddLineToPoint(c->ctx,(float)pts->x,(float)pts->y);
      pts++;
    }
  }
//  CGContextStrokePath(c->ctx);
}
void *SWELL_GetCtxGC(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return 0;
  return NULL;
}


void SWELL_SetPixel(HDC ctx, int x, int y, int c)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return;
  /* CGContextBeginPath(ct->ctx);
   CGContextMoveToPoint(ct->ctx,(float)x,(float)y);
   CGContextAddLineToPoint(ct->ctx,(float)x+0.5,(float)y+0.5);
   CGContextSetLineWidth(ct->ctx,(float)1.5);
   CGContextSetRGBStrokeColor(ct->ctx,GetRValue(c)/255.0,GetGValue(c)/255.0,GetBValue(c)/255.0,1.0);
   CGContextStrokePath(ct->ctx);
  */
}


BOOL GetTextMetrics(HDC ctx, TEXTMETRIC *tm)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (tm) // give some sane defaults
  {
    tm->tmInternalLeading=0;
    tm->tmAscent=8;
    tm->tmDescent=0;
    tm->tmHeight=8;
    tm->tmAveCharWidth = 8;
  }
  if (!HDC_VALID(ct)||!tm) return 0;

  return 1;
}


int DrawText(HDC ctx, const char *buf, int buflen, RECT *r, int align)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!r) return 0;

#ifdef SWELL_FREETYPE
#else
  const int lineh = 8;
  const int charw = 8;
  if (align&DT_CALCRECT)
  {
    if (align&DT_SINGLELINE)
    {
      r->right = r->left + ( buflen < 0 ? strlen(buf) : buflen ) *charw;
      int h = r->right ? lineh:0;
      r->bottom = r->top+h;
      return h;
    }
    int xpos=0;
    r->bottom=r->top;
    bool in_prefix=false;
    while (buflen--) // if buflen<0, go forever
    {
      char c = *buf++;
      if (!c) break;
      if (!xpos) r->bottom += lineh;

      if (c=='&' && !in_prefix && !(align&DT_NOPREFIX))
      {
        in_prefix = true;
        continue;
      }
      in_prefix=false;

      if (c == '\n') xpos=0;
      else if (c == '\r') xpos+=0;
      else if (c =='\t') xpos+=charw*5;
      else xpos += charw;

      if (r->left+xpos>r->right) r->right=r->left+xpos;
    }
    return r->bottom-r->top;
  }
  if (!HDC_VALID(ct)) return 0;

  RECT use_r = *r;
  use_r.left += ct->surface_offs.x;
  use_r.right += ct->surface_offs.x;
  use_r.top += ct->surface_offs.y;
  use_r.bottom += ct->surface_offs.y;

  int xpos = use_r.left;
  int ypos = use_r.top;
  if (align&(DT_CENTER|DT_VCENTER|DT_RIGHT|DT_BOTTOM))
  {
    RECT tr= {0,};
    DrawText(ctx,buf,buflen,&tr,align|DT_CALCRECT);

    if (align&DT_CENTER)
      xpos -= ((tr.right-tr.left) - (use_r.right-use_r.left))/2;
    else if (align&DT_RIGHT)
      xpos = use_r.right - (tr.right-tr.left);

    if (align&DT_VCENTER)
      ypos -= ((tr.bottom-tr.top) - (use_r.bottom-use_r.top))/2;
    else if (align&DT_BOTTOM)
      ypos = use_r.bottom - (tr.bottom-tr.top);
  }
  LICE_IBitmap *surface = ct->surface;
  int fgcol = ct->cur_text_color_int;
  int bgcol = ct->curbkcol;
  int bgmode = ct->curbkmode;


  int clip_x1=max(use_r.left,0), clip_y1 = max(use_r.top,0);
  int clip_w=0, clip_h=0;
  if (surface)
  {
    clip_w = min(use_r.right,surface->getWidth())-clip_x1;
    clip_h = min(use_r.bottom,surface->getHeight())-clip_y1;
    if (clip_w<0)clip_w=0;
    if (clip_h<0)clip_h=0;
  }

  LICE_SubBitmap clipbm(surface,clip_x1,clip_y1,clip_w,clip_h);
  if (surface && !(align&DT_NOCLIP)) {
    surface = &clipbm;
    xpos-=clip_x1;
    ypos-=clip_y1;
  }

  int left_xpos = xpos,ysize=0,max_xpos=0;
  int start_ypos = ypos;
  bool in_prefix=false;

  while (buflen--)
  {
    char c = *buf++;
    if (!c) break;
    if (xpos==left_xpos) ysize+=lineh;

    bool doUl=in_prefix;

    if (c=='&' && !in_prefix && !(align&DT_NOPREFIX))
    {
      in_prefix = true;
      continue;
    }
    in_prefix=false;

    if (c=='\n' && !(align&DT_SINGLELINE)) {
      xpos=left_xpos;
      ypos+=lineh;
    }
    else if (c=='\r')  {}
    else if (c=='\t')
    {
      if (bgmode==OPAQUE) LICE_FillRect(surface,xpos,ypos,charw*5,lineh,bgcol,1.0f,LICE_BLIT_MODE_COPY);
      xpos+=charw*5;
    }
    else
    {
      if (bgmode==OPAQUE) LICE_FillRect(surface,xpos,ypos,charw,lineh,bgcol,1.0f,LICE_BLIT_MODE_COPY);
      LICE_DrawChar(surface,xpos,ypos,c,fgcol,1.0f,LICE_BLIT_MODE_COPY);
      if (doUl) LICE_Line(surface,xpos,ypos+lineh+1,xpos+charw,ypos+lineh+1,fgcol,1.0f,LICE_BLIT_MODE_COPY,false);

      xpos+=charw;
    }
    if(xpos>max_xpos)max_xpos=xpos;
  }
  if (surface==&clipbm)
    swell_DirtyContext(ct,clip_x1+left_xpos,clip_y1+start_ypos,clip_x1+max_xpos,clip_y1+start_ypos+ysize);
  else
    swell_DirtyContext(ct,left_xpos,start_ypos,max_xpos,start_ypos+ysize);
  return ysize;
#endif
}


int GetTextColor(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return -1;
  return ct->cur_text_color_int;
}
void SetTextColor(HDC ctx, int col)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (!HDC_VALID(ct)) return;
  ct->cur_text_color_int = LICE_RGBA_FROMNATIVE(col,255);

}


////////// todo: some sort of HICON emul

HICON LoadNamedImage(const char *name, bool alphaFromMask)
{
  return 0; // todo
}

void DrawImageInRect(HDC ctx, HICON img, RECT *r)
{
  // todo
}


BOOL GetObject(HICON icon, int bmsz, void *_bm)
{
  memset(_bm,0,bmsz);
  if (bmsz != sizeof(BITMAP)) return false;
  BITMAP *bm=(BITMAP *)_bm;
  HGDIOBJ__ *i = (HGDIOBJ__ *)icon;
  if (!HGDIOBJ_VALID(i,TYPE_BITMAP)) return false;

  return false;
  /*
    NSImage *img = i->bitmapptr;
    if (!img) return false;
    bm->bmWidth = (int) ([img size].width+0.5);
    bm->bmHeight = (int) ([img size].height+0.5);
    return true;
  */
}


////////////////////////////////////

#define ColorFromNSColor(a,b) (b)
int GetSysColor(int idx)
{
// NSColors that seem to be valid: textBackgroundColor, selectedTextBackgroundColor, textColor, selectedTextColor
  switch (idx)
  {
  case COLOR_WINDOW:
    return ColorFromNSColor([NSColor controlColor],RGB(192,192,192));
  case COLOR_3DFACE:
  case COLOR_BTNFACE:
    return ColorFromNSColor([NSColor controlColor],RGB(192,192,192));
  case COLOR_SCROLLBAR:
    return ColorFromNSColor([NSColor controlColor],RGB(32,32,32));
  case COLOR_3DSHADOW:
    return ColorFromNSColor([NSColor selectedTextBackgroundColor],RGB(96,96,96));
  case COLOR_3DHILIGHT:
    return ColorFromNSColor([NSColor selectedTextBackgroundColor],RGB(224,224,224));
  case COLOR_BTNTEXT:
    return ColorFromNSColor([NSColor selectedTextBackgroundColor],RGB(0,0,0));
  case COLOR_3DDKSHADOW:
    return (ColorFromNSColor([NSColor selectedTextBackgroundColor],RGB(96,96,96))>>1)&0x7f7f7f;
  case COLOR_INFOBK:
    return RGB(255,240,200);
  case COLOR_INFOTEXT:
    return RGB(0,0,0);

  }
  return 0;
}

void BitBltAlphaFromMem(HDC hdcOut, int x, int y, int w, int h, void *inbufptr, int inbuf_span, int inbuf_h, int xin, int yin, int mode, bool useAlphaChannel, float opacity)
{
// todo: use LICE_WrapperBitmap?
}

void BitBltAlpha(HDC hdcOut, int x, int y, int w, int h, HDC hdcIn, int xin, int yin, int mode, bool useAlphaChannel, float opacity)
{
  HDC__ *in = (HDC__ *)hdcIn;
  HDC__ *out = (HDC__ *)hdcOut;
  if (!HDC_VALID(out) || !HDC_VALID(in)) return;
  if (!in->surface || !out->surface) return;
  LICE_Blit(out->surface,in->surface,
            x+out->surface_offs.x,y+out->surface_offs.y,
            xin+in->surface_offs.x,yin+in->surface_offs.y,w,h,
            opacity,LICE_BLIT_MODE_COPY|(useAlphaChannel?LICE_BLIT_USE_ALPHA:0));
  swell_DirtyContext(out,x,y,x+w,y+h);
}

void BitBlt(HDC hdcOut, int x, int y, int w, int h, HDC hdcIn, int xin, int yin, int mode)
{
  HDC__ *in = (HDC__ *)hdcIn;
  HDC__ *out = (HDC__ *)hdcOut;
  if (!HDC_VALID(out) || !HDC_VALID(in)) return;
  if (!in->surface || !out->surface) return;
  LICE_Blit(out->surface,in->surface,
            x+out->surface_offs.x,y+out->surface_offs.y,
            xin+in->surface_offs.x,yin+in->surface_offs.y,w,h,
            1.0f,LICE_BLIT_MODE_COPY);
  swell_DirtyContext(out,x,y,x+w,y+h);
}

void StretchBlt(HDC hdcOut, int x, int y, int w, int h, HDC hdcIn, int xin, int yin, int srcw, int srch, int mode)
{
}

void SWELL_FillDialogBackground(HDC hdc, RECT *r, int level)
{
}

void SWELL_PushClipRegion(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
//  if (ct && ct->ctx) CGContextSaveGState(ct->ctx);
}

void SWELL_SetClipRegion(HDC ctx, RECT *r)
{
  HDC__ *ct=(HDC__ *)ctx;
//  if (ct && ct->ctx) CGContextClipToRect(ct->ctx,CGRectMake(r->left,r->top,r->right-r->left,r->bottom-r->top));

}

void SWELL_PopClipRegion(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
//  if (ct && ct->ctx) CGContextRestoreGState(ct->ctx);
}

void *SWELL_GetCtxFrameBuffer(HDC ctx)
{
  HDC__ *ct=(HDC__ *)ctx;
  if (HDC_VALID(ct)&&ct->surface) return ct->surface->getBits();
  return 0;
}



struct swell_gdpLocalContext
{
  HDC__ ctx;
  RECT clipr;
};



HDC SWELL_internalGetWindowDC(HWND h, bool calcsize_on_first)
{
  if (!h) return NULL;

  int xoffs=0,yoffs=0;
  int wndw = h->m_position.right-h->m_position.left;
  int wndh = h->m_position.bottom-h->m_position.top;

  HWND starth = h;
  while (h)
  {
    if ((calcsize_on_first || h!=starth)  && h->m_wndproc)
    {
      RECT r,r2;
      GetWindowContentViewRect(h,&r);
      r2=r;
      h->m_wndproc(h,WM_NCCALCSIZE,FALSE,(LPARAM)&r);

      // todo: handle left edges and adjust postions/etc accordingly
      if (h==starth)  // if initial window, adjust rects to client area (this implies calcsize_on_first being false)
      {
        wndw=r.right-r.left;
        wndh=r.bottom-r.top;
      }
      yoffs += r.top-r2.top;
      xoffs += r.left-r2.left;
    }
    if (h->m_backingstore) break; // found our target window

    xoffs += h->m_position.left;
    yoffs += h->m_position.top;
    h = h->m_parent;
  }
  if (!h) return NULL;

  swell_gdpLocalContext *p = (swell_gdpLocalContext*)SWELL_GDP_CTX_NEW();
  // todo: GDI defaults?
  p->ctx.surface=new LICE_SubBitmap(h->m_backingstore,xoffs,yoffs,wndw,wndh);
  if (xoffs<0) p->ctx.surface_offs.x = xoffs;
  if (yoffs<0) p->ctx.surface_offs.y = yoffs;
  p->clipr.left=xoffs;
  p->clipr.top=yoffs;
  p->clipr.right=xoffs + p->ctx.surface->getWidth();
  p->clipr.bottom=yoffs + p->ctx.surface->getHeight();

  return (HDC)p;
}
HDC GetWindowDC(HWND h)
{
  return SWELL_internalGetWindowDC(h,0);
}

HDC GetDC(HWND h)
{
  return SWELL_internalGetWindowDC(h,1);
}

void ReleaseDC(HWND h, HDC hdc)
{
  if (!h || !HDC_VALID(hdc)) return;
  swell_gdpLocalContext *p = (swell_gdpLocalContext*)hdc;


  if (!h->m_paintctx)
  {
    // handle blitting?
    HWND par = h;
    while (par && !par->m_backingstore) par=par->m_parent;
    void swell_OSupdateWindowToScreen(HWND hwnd, RECT *rect);
    if (par)
    {
      if (p->ctx.dirty_rect_valid)
      {
        RECT r = p->clipr, dr=p->ctx.dirty_rect;
        dr.left += r.left;
        dr.top += r.top;
        dr.right += r.left;
        dr.bottom += r.top;
#if 1
        if (dr.left > r.left) r.left=dr.left;
        if (dr.top > r.top) r.top=dr.top;
        if (dr.right < r.right) r.right=dr.right;
        if (dr.bottom < r.bottom) r.bottom=dr.bottom;
#endif

        if (r.top<r.bottom && r.left<r.right) swell_OSupdateWindowToScreen(par,&r);
      }
    }
  }
  delete p->ctx.surface;
  SWELL_GDP_CTX_DELETE(hdc);
}



HDC BeginPaint(HWND hwnd, PAINTSTRUCT *ps)
{
  if (!ps) return 0;
  memset(ps,0,sizeof(PAINTSTRUCT));
  if (!hwnd) return 0;
  if (!hwnd->m_paintctx) return NULL;

  swell_gdpLocalContext *ctx = (swell_gdpLocalContext *)hwnd->m_paintctx;
  ps->rcPaint = ctx->clipr;
  ps->hdc = &ctx->ctx;
  return &ctx->ctx;
}


// paint hwnd into bmout, where bmout points at bmout_xpos,bmout_ypos in window coordinates
void SWELL_internalLICEpaint(HWND hwnd, LICE_IBitmap *bmout, int bmout_xpos, int bmout_ypos, bool forceref)
{
  // todo: check to see if any children completely intersect clip region, if so then we don't need to render ourselves
  // todo: opaque flag for windows? (to disable above behavior)
  // todo: implement/use per-window backing store.
  //   we would want to only really use them for top level windows, and on those, only update windows (and children of windows) who had backingstore invalidated.
  if (hwnd->m_invalidated) forceref=true;

  if (forceref || hwnd->m_child_invalidated)
  {
    swell_gdpLocalContext ctx= {0,}; // todo set up gdi context defaults?
    ctx.ctx.surface = bmout;
    ctx.ctx.surface_offs.x = -bmout_xpos;
    ctx.ctx.surface_offs.y = -bmout_ypos;
    ctx.clipr.left = bmout_xpos;
    ctx.clipr.top = bmout_ypos;
    ctx.clipr.right = ctx.clipr.left + bmout->getWidth();
    ctx.clipr.bottom = ctx.clipr.top + bmout->getHeight();

    void *oldpaintctx = hwnd->m_paintctx;
    if (forceref) hwnd->m_paintctx = &ctx;

    LICE_SubBitmap tmpsub(NULL,0,0,0,0);
    if (hwnd->m_wndproc) // this happens after m_paintctx is set -- that way GetWindowDC()/GetDC()/ReleaseDC() know to not actually update the screen
    {
      RECT r,r2;
      GetWindowContentViewRect(hwnd,&r);
      r.right-=r.left;
      r.bottom-=r.top;
      r.left=r.top=0;
      r2=r;
      hwnd->m_wndproc(hwnd,WM_NCCALCSIZE,FALSE,(LPARAM)&r);
      if (forceref) hwnd->m_wndproc(hwnd,WM_NCPAINT,(WPARAM)1,0);

      if (r.left!=r2.left) {} // todo: adjust drawing offsets, clip rects, accordingly
      if (r.top!=r2.top) {} // todo: adjust drawing offsets, clip rects, accordingly
      int dx = r.left-r2.left,dy=r.top-r2.top;
      // dx,dy is offset from the window's root of
      bmout_xpos -= dx;
      bmout_ypos -= dy;

      if (dx||dy)
      {
        tmpsub.m_parent = ctx.ctx.surface;
        tmpsub.m_x = dx;
        tmpsub.m_y = dy;
        tmpsub.m_w = ctx.ctx.surface->getWidth()-dx;
        tmpsub.m_h = ctx.ctx.surface->getHeight()-dy;
        if (tmpsub.m_w<0) tmpsub.m_w=0;
        if (tmpsub.m_h<0) tmpsub.m_h=0;
        ctx.ctx.surface = &tmpsub;

        ctx.clipr.left-=dx;
        ctx.clipr.top-=dy;
        ctx.clipr.right-=dx;
        ctx.clipr.bottom-=dy;
      }

      // adjust clip rects for right/bottom extents
      int newr = r.right-r.left;
      if (ctx.clipr.right > newr) ctx.clipr.right=newr;
      int newb = r.bottom-r.top;
      if (ctx.clipr.bottom > newb) ctx.clipr.bottom=newb;
    }

    // paint
    if (forceref)
    {
      if (hwnd->m_wndproc && ctx.clipr.right > ctx.clipr.left && ctx.clipr.bottom > ctx.clipr.top)
      {
        hwnd->m_wndproc(hwnd,WM_PAINT,(WPARAM)&ctx,0);
      }

      hwnd->m_paintctx = oldpaintctx;

      // it might be good to blit here on some OSes, rather than from the top level caller...
      hwnd->m_invalidated=false;
    }
  }

  if (forceref || hwnd->m_child_invalidated)
  {
    HWND h = hwnd->m_children;
    while (h && h->m_next) h=h->m_next;
    while (h)  // go through list backwards (first in list = top of Z order)
    {
      if (h->m_visible && (forceref || h->m_invalidated||h->m_child_invalidated))
      {
        int width = h->m_position.right - h->m_position.left, height = h->m_position.bottom - h->m_position.top; // max width possible for this window
        int xp = h->m_position.left - bmout_xpos, yp = h->m_position.top - bmout_ypos;

        // if xp/yp < 0, that means that the clip region starts inside the window, so we need to pass a positive render offset, and decrease the potential draw width
        // if xp/yp > 0, then the clip region starts before the window, so we use the subbitmap and pass 0 for the offset parm
        int subx = 0, suby=0;
        if (xp<0) width+=xp;
        else {
          subx=xp;
          xp=0;
        }

        if (yp<0) height+=yp;
        else {
          suby=yp;
          yp=0;
        }

        LICE_SubBitmap subbm(bmout,subx,suby,width,height); // the right/bottom will automatically be clipped to the clip rect etc
        if (subbm.getWidth()>0 && subbm.getHeight()>0)
          SWELL_internalLICEpaint(h,&subbm,-xp,-yp,forceref);
      }
      h = h->m_prev;
    }
  }
  hwnd->m_child_invalidated=false;
}

HBITMAP CreateBitmap(int width, int height, int numplanes, int bitsperpixel, unsigned char* bits)
{
  return NULL;
}

HICON CreateIconIndirect(ICONINFO* iconinfo)
{
  if (!iconinfo || !iconinfo->fIcon) return 0;
  HGDIOBJ__* i=iconinfo->hbmColor;
  if (!HGDIOBJ_VALID(i,TYPE_BITMAP) ) return 0;
  /*
    if (!i->bitmapptr) return 0;
    HGDIOBJ__* icon=GDP_OBJECT_NEW();
    icon->type=TYPE_BITMAP;
    icon->wid=1;
    return icon;
  */
  return NULL;
}

#endif

#endif // SWELL_LICE_GDI
