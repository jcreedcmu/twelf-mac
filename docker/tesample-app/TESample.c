// We are trying to compile with multiversal interface
#define UNIVERSAL_INTERFACE 0

#include <Devices.h>
#include <Dialogs.h>
#include <Events.h>
#include <Files.h>
#include <Fonts.h>
#include <Memory.h>
#include <Menus.h>
#include <OSUtils.h>
#include <Quickdraw.h>
#include <SegLoad.h>
#include <TextEdit.h>
#include <ToolUtils.h>
#include <Traps.h>
#include <Types.h>
#include <Windows.h>
#include <limits.h>
#include <stdio.h>

#include "api.h"
#include "asmclikloop.h"
#include "clikloop.h"
#include "consts.h"
#include "document.h"
#include "resource-consts.h"
#include "scrolling.h"
#include "view-rects.h"

#if UNIVERSAL_INTERFACE
#include <ControlDefinitions.h>
#include <Controls.h>
#include <DiskInit.h>
#include <Packages.h>
#include <Scrap.h>
#endif

void AlertUser(short error);
void EventLoop(void);
void DoEvent(EventRecord *event);
void AdjustCursor(Point mouse, RgnHandle region);
void GetGlobalMouse(Point *mouse);
void DoGrowWindow(WindowPtr window, EventRecord *event);
void DoZoomWindow(WindowPtr window, short part);
void _ResizeWindow(WindowPtr window);
void GetLocalUpdateRgn(WindowPtr window, RgnHandle localRgn);
void DoUpdate(WindowPtr window);
void DoDeactivate(WindowPtr window);
void DoActivate(WindowPtr window, Boolean becomingActive);
void DoContentClick(WindowPtr window, EventRecord *event);
void DoKeyDown(EventRecord *event);
unsigned long GetSleep(void);
void DoIdle(void);
void DrawWindow(WindowPtr window);
void AdjustMenus(void);
void DoMenuCommand(long menuResult);
void DoNew(void);
Boolean DoCloseWindow(WindowPtr window);
void Terminate(void);
void Initialize(void);
void BigBadError(short error);
Boolean IsAppWindow(WindowPtr window);
Boolean IsDAWindow(WindowPtr window);
Boolean TrapAvailable(short tNumber, TrapType tType);

/* The "g" prefix is used to emphasize that a variable is global. */

/* GMac is used to hold the result of a SysEnvirons call. This makes
   it convenient for any routine to check the environment. It is
   global information, anyway. */
SysEnvRec gMac; /* set up by Initialize */

/* GHasWaitNextEvent is set at startup, and tells whether the WaitNextEvent
   trap is available. If it is false, we know that we must call GetNextEvent. */
Boolean gHasWaitNextEvent; /* set up by Initialize */

/* GInBackground is maintained by our OSEvent handling routines. Any part of
   the program can check it to find out if it is currently in the background. */
Boolean gInBackground; /* maintained by Initialize and DoEvent */

/* GNumDocuments is used to keep track of how many open documents there are
   at any time. It is maintained by the routines that open and close documents.
 */
short gNumDocuments; /* maintained by Initialize, DoNew, and DoCloseWindow */

/* Define TopLeft and BotRight macros for convenience. Notice the implicit
   dependency on the ordering of fields within a Rect */
#define TopLeft(aRect) (*(Point *)&(aRect).top)
#define BotRight(aRect) (*(Point *)&(aRect).bottom)

/*	Set up the whole world, including global variables, Toolbox managers,
        menus, and a single blank document. */

/*	1.01 - The code that used to be part of ForceEnvirons has been moved
   into this module. If an error is detected, instead of merely doing an
   ExitToShell, which leaves the user without much to go on, we call AlertUser,
   which puts up a simple alert that just says an error occurred and then calls
   ExitToShell. Since there is no other cleanup needed at this point if an error
   is detected, this form of error- handling is acceptable. If more
   sophisticated error recovery is needed, an exception mechanism, such as is
   provided by Signals, can be used. */

void Initialize() {
  Handle menuBar;
  long total, contig;
  EventRecord event;
  short count;

  gInBackground = false;

  InitGraf(&qd.thePort);
  InitFonts();
  InitWindows();
  InitMenus();
  TEInit();
  InitDialogs(nil);
  InitCursor();

  /*	Call MPPOpen and ATPLoad at this point to initialize AppleTalk,
          if you are using it. */
  /*	NOTE -- It is no longer necessary, and actually unhealthy, to check
          PortBUse and SPConfig before opening AppleTalk. The drivers are
     capable of checking for port availability themselves. */

  /*	This next bit of code is necessary to allow the default button of our
          alert be outlined.
          1.02 - Changed to call EventAvail so that we don't lose some important
          events. */

  for (count = 1; count <= 3; count++) EventAvail(everyEvent, &event);

  /*	Ignore the error returned from SysEnvirons; even if an error occurred,
          the SysEnvirons glue will fill in the SysEnvRec. You can save a
     redundant call to SysEnvirons by calling it after initializing AppleTalk.
   */

  SysEnvirons(kSysEnvironsVersion, &gMac);

  /* Make sure that the machine has at least 128K ROMs. If it doesn't, exit. */

  if (gMac.machineType < 0) BigBadError(eWrongMachine);

  /*	1.02 - Move TrapAvailable call to after SysEnvirons so that we can tell
          in TrapAvailable if a tool trap value is out of range. */

  gHasWaitNextEvent = TrapAvailable(_WaitNextEvent, ToolTrap);

  /*	1.01 - We used to make a check for memory at this point by examining
     ApplLimit, ApplicationZone, and StackSpace and comparing that to the
     minimum size we told MultiFinder we needed. This did not work well because
     it assumed too much about the relationship between what we asked
     MultiFinder for and what we would actually get back, as well as how to
     measure it. Instead, we will use an alternate method comprised of two
     steps. */

  /*	It is better to first check the size of the application heap against a
     value that you have determined is the smallest heap the application can
     reasonably work in. This number should be derived by examining the size of
     the heap that is actually provided by MultiFinder when the minimum size
     requested is used. The derivation of the minimum size requested from
     MultiFinder is described in Sample.h. The check should be made because the
     preferred size can end up being set smaller than the minimum size by the
     user. This extra check acts to insure that your application is starting
     from a solid memory foundation. */

#if UNIVERSAL_INTERFACE
  if ((long)GetApplLimit() - (long)ApplicationZone() < kMinHeap)
    BigBadError(eSmallSize);
#endif

  /*	Next, make sure that enough memory is free for your application to run.
     It is possible for a situation to arise where the heap may have been of
     required size, but a large scrap was loaded which left too little memory.
     To check for this, call PurgeSpace and compare the result with a value that
     you have determined is the minimum amount of free memory your application
     needs at initialization. This number can be derived several different ways.
     One way that is fairly straightforward is to run the application in the
     minimum size configuration as described previously. Call PurgeSpace at
     initialization and examine the value returned. However, you should make
     sure that this result is not being modified by the scrap's presence. You
     can do that by calling ZeroScrap before calling PurgeSpace. Make sure to
     remove that call before shipping, though. */

  /* ZeroScrap(); */

  PurgeSpace(&total, &contig);
  if (total < kMinSpace)
    if (UnloadScrap() != noErr)
      BigBadError(eNoMemory);
    else {
      PurgeSpace(&total, &contig);
      if (total < kMinSpace) BigBadError(eNoMemory);
    }

  /*	The extra benefit to waiting until after the Toolbox Managers have been
     initialized to check memory is that we can now give the user an alert to
     tell him/her what happened. Although it is possible that the memory
     situation could be worsened by displaying an alert, MultiFinder would
     gracefully exit the application with an informative alert if memory became
     critical. Here we are acting more in a preventative manner to avoid future
     disaster from low-memory problems. */

  menuBar = GetNewMBar(rMenuBar); /* read menus into menu bar */
  if (menuBar == nil) BigBadError(eNoMemory);
  SetMenuBar(menuBar); /* install menus */
  DisposeHandle(menuBar);
  AppendResMenu(GetMenuHandle(mApple), 'DRVR'); /* add DA names to Apple menu */
  DrawMenuBar();

  gNumDocuments = 0;

  /* do other initialization here */

  DoNew(); /* create a single empty document */
} /*Initialize*/

/* Used whenever a, like, fully fatal error happens */
void BigBadError(short error) {
  AlertUser(error);
  ExitToShell();
}

/*	Check to see if a given trap is implemented. This is only used by the
        Initialize routine in this program, so we put it in the Initialize
   segment. The recommended approach to see if a trap is implemented is to see
   if the address of the trap routine is the same as the address of the
        Unimplemented trap. */
/*	1.02 - Needs to be called after call to SysEnvirons so that it can check
        if a ToolTrap is out of range of a pre-MacII ROM. */

Boolean TrapAvailable(short tNumber, TrapType tType) {
  if ((tType == (unsigned char)ToolTrap) &&
      (gMac.machineType > envMachUnknown) &&
      (gMac.machineType < envMacII)) { /* it's a 512KE, Plus, or SE */
    tNumber = tNumber & 0x03FF;
    if (tNumber > 0x01FF)       /* which means the tool traps */
      tNumber = _Unimplemented; /* only go to 0x01FF */
  }
  return NGetTrapAddress(tNumber, tType) !=
         NGetTrapAddress(_Unimplemented, ToolTrap);
} /*TrapAvailable*/

int main() {
  // Debugging Log
  stdout = stderr = fopen("out", "w");
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  /* 1.01 - call to ForceEnvirons removed */

  /*	If you have stack requirements that differ from the default,
          then you could use SetApplLimit to increase StackSpace at
          this point, before calling MaxApplZone. */
  MaxApplZone(); /* expand the heap so code segments load at the top */

  Initialize();               /* initialize the program */
  UnloadSeg((Ptr)Initialize); /* note that Initialize must not be in Main! */

  EventLoop(); /* call the main event loop */
}

/* Get events forever, and handle them by calling DoEvent.
   Also call AdjustCursor each time through the loop. */

void EventLoop() {
  RgnHandle cursorRgn;
  Boolean gotEvent;
  EventRecord event;
  Point mouse;

  cursorRgn = NewRgn(); /* we'll pass WNE an empty region the 1st time thru */
  do {
    /* use WNE if it is available */
    if (gHasWaitNextEvent) {
      GetGlobalMouse(&mouse);
      AdjustCursor(mouse, cursorRgn);
      gotEvent = WaitNextEvent(everyEvent, &event, GetSleep(), cursorRgn);
    } else {
      SystemTask();
      gotEvent = GetNextEvent(everyEvent, &event);
    }
    if (gotEvent) {
      /* make sure we have the right cursor before handling the event */
      AdjustCursor(event.where, cursorRgn);
      DoEvent(&event);
    } else
      DoIdle();   /* perform idle tasks when it's not our event */
                  /*	If you are using modeless dialogs that have editText items,
                          you will want to call IsDialogEvent to give the caret a chance
                          to blink, even if WNE/GNE returned FALSE. However, check FrontWindow
                          for a non-NIL value before calling IsDialogEvent. */
  } while (true); /* loop forever; we quit via ExitToShell */
} /*EventLoop*/

/* Do the right thing for an event. Determine what kind of event it is, and call
 the appropriate routines. */

void DoEvent(EventRecord *event) {
  short part, err;
  WindowPtr window;
  char key;
  Point aPoint;

  switch (event->what) {
    case nullEvent:
      /* we idle for null/mouse moved events ands for events which aren't
              ours (see EventLoop) */
      DoIdle();
      break;
    case mouseDown:
      part = FindWindow(event->where, &window);
      switch (part) {
        case inMenuBar:  /* process a mouse menu command (if any) */
          AdjustMenus(); /* bring 'em up-to-date */
          DoMenuCommand(MenuSelect(event->where));
          break;
        case inSysWindow: /* let the system handle the mouseDown */
          SystemClick(event, window);
          break;
        case inContent:
          if (window != FrontWindow()) {
            SelectWindow(window);
            /*DoEvent(event);*/ /* use this line for "do first click" */
          } else
            DoContentClick(window, event);
          break;
        case inDrag: /* pass screenBits.bounds to get all gDevices */
          DragWindow(window, event->where, &qd.screenBits.bounds);
          break;
        case inGoAway:
          if (TrackGoAway(window, event->where))
            DoCloseWindow(window); /* we don't care if the user cancelled */
          break;
        case inGrow:
          DoGrowWindow(window, event);
          break;
        case inZoomIn:
        case inZoomOut:
          if (TrackBox(window, event->where, part)) DoZoomWindow(window, part);
          break;
      }
      break;
    case keyDown:
    case autoKey: /* check for menukey equivalents */
      key = event->message & charCodeMask;
      if (event->modifiers & cmdKey) { /* Command key down */
        if (event->what == keyDown) {
          AdjustMenus(); /* enable/disable/check menu items properly */
          DoMenuCommand(MenuKey(key));
        }
      } else
        DoKeyDown(event);
      break;
    case activateEvt:
      DoActivate((WindowPtr)event->message,
                 (event->modifiers & activeFlag) != 0);
      break;
    case updateEvt:
      DoUpdate((WindowPtr)event->message);
      break;
    /*	1.01 - It is not a bad idea to at least call DIBadMount in response
            to a diskEvt, so that the user can format a floppy. */
    case diskEvt:

      if (HiWord(event->message) != noErr) {
        SetPt(&aPoint, kDILeft, kDITop);
        err = DIBadMount(aPoint, event->message);
      }

      break;
    case kOSEvent:
      /*	1.02 - must BitAND with 0x0FF to get only low byte */
      switch ((event->message >> 24) & 0x0FF) { /* high byte of message */
        case kMouseMovedMessage:
          DoIdle(); /* mouse-moved is also an idle event */
          break;
        case kSuspendResumeMessage: /* suspend/resume is also an
                                       activate/deactivate */
          gInBackground = (event->message & kResumeMask) == 0;
          DoActivate(FrontWindow(), !gInBackground);
          break;
      }
      break;
  }
} /*DoEvent*/

/*	Change the cursor's shape, depending on its position. This also
   calculates the region where the current cursor resides (for WaitNextEvent).
   When the mouse moves outside of this region, an event is generated. If there
   is more to the event than just `the mouse moved', we get called before the
   event is processed to make sure the cursor is the right one. In any (ahem)
   event, this is called again before we fall back into WNE. */

void AdjustCursor(Point mouse, RgnHandle region) {
  WindowPtr window;
  RgnHandle arrowRgn;
  RgnHandle iBeamRgn;
  Rect iBeamRect;

  window = FrontWindow(); /* we only adjust the cursor when we are in front */
  if ((!gInBackground) && (!IsDAWindow(window))) {
    /* calculate regions for different cursor shapes */
    arrowRgn = NewRgn();
    iBeamRgn = NewRgn();

    /* start arrowRgn wide open */
    SetRectRgn(arrowRgn, kExtremeNeg, kExtremeNeg, kExtremePos, kExtremePos);

    /* calculate iBeamRgn */
    if (IsAppWindow(window)) {
      iBeamRect = (*((DocumentPeek)window)->docTE)->viewRect;
      SetPort(window); /* make a global version of the viewRect */
      LocalToGlobal(&TopLeft(iBeamRect));
      LocalToGlobal(&BotRight(iBeamRect));
      RectRgn(iBeamRgn, &iBeamRect);
      /* we temporarily change the port's origin to 'globalify' the visRgn */
      SetOrigin(-window->portBits.bounds.left, -window->portBits.bounds.top);
      SectRgn(iBeamRgn, window->visRgn, iBeamRgn);
      SetOrigin(0, 0);
    }

    /* subtract other regions from arrowRgn */
    DiffRgn(arrowRgn, iBeamRgn, arrowRgn);

    /* change the cursor and the region parameter */
    if (PtInRgn(mouse, iBeamRgn)) {
      SetCursor(*GetCursor(iBeamCursor));
      CopyRgn(iBeamRgn, region);
    } else {
      SetCursor(&qd.arrow);
      CopyRgn(arrowRgn, region);
    }

    DisposeRgn(arrowRgn);
    DisposeRgn(iBeamRgn);
  }
} /*AdjustCursor*/

/*	Get the global coordinates of the mouse. When you call OSEventAvail
        it will return either a pending event or a null event. In either case,
        the where field of the event record will contain the current position
        of the mouse in global coordinates and the modifiers field will reflect
        the current state of the modifiers. Another way to get the global
        coordinates is to call GetMouse and LocalToGlobal, but that requires
        being sure that thePort is set to a valid port. */

void GetGlobalMouse(Point *mouse) {
  EventRecord event;

  OSEventAvail(kNoEvents, &event); /* we aren't interested in any events */
  *mouse = event.where;            /* just the mouse position */
} /*GetGlobalMouse*/

/*	Called when a mouseDown occurs in the grow box of an active window. In
        order to eliminate any 'flicker', we want to invalidate only what is
        necessary. Since _ResizeWindow invalidates the whole portRect, we save
        the old TE viewRect, intersect it with the new TE viewRect, and
        remove the result from the update region. However, we must make sure
        that any old update region that might have been around gets put back. */

void DoGrowWindow(WindowPtr window, EventRecord *event) {
  long growResult;
  Rect tempRect;
  RgnHandle tempRgn;
  DocumentPeek doc;

  tempRect = qd.screenBits.bounds; /* set up limiting values */
  tempRect.left = kMinDocDim;
  tempRect.top = kMinDocDim;
  growResult = GrowWindow(window, event->where, &tempRect);
  /* see if it really changed size */
  if (growResult != 0) {
    doc = (DocumentPeek)window;
    tempRect = (*doc->docTE)->viewRect; /* save old text box */
    tempRgn = NewRgn();
    GetLocalUpdateRgn(window, tempRgn); /* get localized update region */
    SizeWindow(window, LoWord(growResult), HiWord(growResult), true);
    _ResizeWindow(window);
    /* calculate & validate the region that hasn't changed so it won't get
     * redrawn */
    SectRect(&tempRect, &(*doc->docTE)->viewRect, &tempRect);
    ValidRect(&tempRect); /* take it out of update */
    InvalRgn(tempRgn);    /* put back any prior update */
    DisposeRgn(tempRgn);
  }
} /* DoGrowWindow */

/* 	Called when a mouseClick occurs in the zoom box of an active window.
        Everything has to get re-drawn here, so we don't mind that
        _ResizeWindow invalidates the whole portRect. */

void DoZoomWindow(WindowPtr window, short part) {
  EraseRect(&window->portRect);
  ZoomWindow(window, part, window == FrontWindow());
  _ResizeWindow(window);
} /*  DoZoomWindow */

/* Called when the window has been resized to fix up the controls and content.
 */
void _ResizeWindow(WindowPtr window) {
  AdjustScrollbars(window, true);
  AdjustTE(window);
  InvalRect(&window->portRect);
} /* _ResizeWindow */

/* Returns the update region in local coordinates */
void GetLocalUpdateRgn(WindowPtr window, RgnHandle localRgn) {
  CopyRgn(((WindowPeek)window)->updateRgn,
          localRgn); /* save old update region */
  OffsetRgn(localRgn, window->portBits.bounds.left,
            window->portBits.bounds.top);
} /* GetLocalUpdateRgn */

/*	This is called when an update event is received for a window.
        It calls DrawWindow to draw the contents of an application window.
        As an efficiency measure that does not have to be followed, it
        calls the drawing routine only if the visRgn is non-empty. This
        will handle situations where calculations for drawing or drawing
        itself is very time-consuming. */

void DoUpdate(WindowPtr window) {
  if (IsAppWindow(window)) {
    BeginUpdate(window);           /* this sets up the visRgn */
    if (!EmptyRgn(window->visRgn)) /* draw if updating needs to be done */
      DrawWindow(window);
    EndUpdate(window);
  }
} /*DoUpdate*/

/*	This is called when a window is activated or deactivated.
        It calls TextEdit to deal with the selection. */

void DoActivate(WindowPtr window, Boolean becomingActive) {
  RgnHandle tempRgn, clipRgn;
  Rect growRect;
  DocumentPeek doc;

  if (IsAppWindow(window)) {
    doc = (DocumentPeek)window;
    if (becomingActive) {
      /*	since we don't want TEActivate to draw a selection in an area
         where we're going to erase and redraw, we'll clip out the update region
              before calling it. */
      tempRgn = NewRgn();
      clipRgn = NewRgn();
      GetLocalUpdateRgn(window, tempRgn); /* get localized update region */
      GetClip(clipRgn);
      DiffRgn(clipRgn, tempRgn, tempRgn); /* subtract updateRgn from clipRgn */
      SetClip(tempRgn);
      TEActivate(doc->docTE);
      SetClip(clipRgn); /* restore the full-blown clipRgn */
      DisposeRgn(tempRgn);
      DisposeRgn(clipRgn);

      /* the controls must be redrawn on activation: */
      (*doc->docVScroll)->contrlVis = kControlVisible;
      (*doc->docHScroll)->contrlVis = kControlVisible;
      InvalRect(&(*doc->docVScroll)->contrlRect);
      InvalRect(&(*doc->docHScroll)->contrlRect);
      /* the growbox needs to be redrawn on activation: */
      growRect = window->portRect;
      /* adjust for the scrollbars */
      growRect.top = growRect.bottom - kScrollbarAdjust;
      growRect.left = growRect.right - kScrollbarAdjust;
      InvalRect(&growRect);
    } else {
      TEDeactivate(doc->docTE);
      /* the controls must be hidden on deactivation: */
      HideControl(doc->docVScroll);
      HideControl(doc->docHScroll);
      /* the growbox should be changed immediately on deactivation: */
      DrawGrowIcon(window);
    }
  }
} /*DoActivate*/

/*	This is called when a mouseDown occurs in the content of a window. */

void DoContentClick(WindowPtr window, EventRecord *event) {
  Point mouse;
  ControlHandle control;
  short part, value;
  Boolean shiftDown;
  DocumentPeek doc;
  Rect teRect;

  if (IsAppWindow(window)) {
    SetPort(window);
    mouse = event->where; /* get the click position */
    GlobalToLocal(&mouse);
    doc = (DocumentPeek)window;
    /* see if we are in the viewRect. if so, we won't check the controls */
    GetTERect(window, &teRect);
    if (PtInRect(mouse, &teRect)) {
      /* see if we need to extend the selection */
      shiftDown =
          (event->modifiers & shiftKey) != 0; /* extend if Shift is down */
      TEClick(mouse, shiftDown, doc->docTE);
    } else {
      part = FindControl(mouse, window, &control);
      switch (part) {
        case 0: /* do nothing for viewRect case */
          break;
        case kControlIndicatorPart:
          value = GetControlValue(control);
          part = TrackControl(control, mouse, nil);
          if (part != 0) {
            value -= GetControlValue(control);
            /* value now has CHANGE in value; if value changed, scroll */
            if (value != 0)
              if (control == doc->docVScroll)
                TEScroll(0, value * (*doc->docTE)->lineHeight, doc->docTE);
              else
                TEScroll(value, 0, doc->docTE);
          }
          break;
        default: /* they clicked in an arrow, so track & scroll */
        {
          if (control == doc->docVScroll) {
            value = TrackControl(control, mouse, VActionProc);
          } else if (control == doc->docHScroll) {
            value = TrackControl(control, mouse, HActionProc);
          }

        } break;
      }
    }
  }
} /*DoContentClick*/

/* This is called for any keyDown or autoKey events, except when the
 Command key is held down. It looks at the frontmost window to decide what
 to do with the key typed. */

void DoKeyDown(EventRecord *event) {
  WindowPtr window;
  char key;
  TEHandle te;

  window = FrontWindow();
  if (IsAppWindow(window)) {
    te = ((DocumentPeek)window)->docTE;
    key = event->message & charCodeMask;
    /* we have a char. for our window; see if we are still below TextEdit's
            limit for the number of characters (but deletes are always rad) */
    if (key == kDelChar ||
        (*te)->teLength - ((*te)->selEnd - (*te)->selStart) + 1 <
            kMaxTELength) {
      TEKey(key, te);
      AdjustScrollbars(window, false);
      AdjustTE(window);
    } else
      AlertUser(eExceedChar);
  }
} /*DoKeyDown*/

/*	Calculate a sleep value for WaitNextEvent. This takes into account the
   things that DoIdle does with idle time. */

unsigned long GetSleep() {
#if UNIVERSAL_INTERFACE
  long sleep;
  WindowPtr window;
  TEHandle te;

  sleep = LONG_MAX; /* default value for sleep */
  if (!gInBackground) {
    window = FrontWindow(); /* and the front window is ours... */
    if (IsAppWindow(window)) {
      te = ((DocumentPeek)(window))
               ->docTE; /* and the selection is an insertion point... */
      if ((*te)->selStart == (*te)->selEnd)
        sleep = GetCaretTime(); /* blink time for the insertion point */
    }
  }
  return sleep;
#else
  return 0;
#endif
} /*GetSleep*/

/* This is called whenever we get a null event et al.
 It takes care of necessary periodic actions. For this program, it calls TEIdle.
 */

void DoIdle() {
  WindowPtr window;

  window = FrontWindow();
  if (IsAppWindow(window)) TEIdle(((DocumentPeek)window)->docTE);
} /*DoIdle*/

/* Draw the contents of an application window. */

void DrawWindow(WindowPtr window) {
  SetPort(window);
  EraseRect(&window->portRect);
  DrawControls(window);
  DrawGrowIcon(window);
  TEUpdate(&window->portRect, ((DocumentPeek)window)->docTE);
} /*DrawWindow*/

/*	Enable and disable menus based on the current state.
        The user can only select enabled menu items. We set up all the menu
   items before calling MenuSelect or MenuKey, since these are the only times
   that a menu item can be selected. Note that MenuSelect is also the only time
        the user will see menu items. This approach to deciding what enable/
        disable state a menu item has the advantage of concentrating all
        the decision-making in one routine, as opposed to being spread
   throughout the application. Other application designs may take a different
   approach that may or may not be as valid. */

void AdjustMenus() {
  WindowPtr window;
  MenuHandle menu;
  long offset;
  Boolean undo;
  Boolean cutCopyClear;
  Boolean paste;
  TEHandle te;

  window = FrontWindow();

  menu = GetMenuHandle(mFile);
  if (gNumDocuments < kMaxOpenDocuments)
    EnableItem(menu, iNew); /* New is enabled when we can open more documents */
  else
    DisableItem(menu, iNew);
  if (window != nil) /* Close is enabled when there is a window to close */
    EnableItem(menu, iClose);
  else
    DisableItem(menu, iClose);

  menu = GetMenuHandle(mEdit);
  undo = false;
  cutCopyClear = false;
  paste = false;
  if (IsDAWindow(window)) {
    undo = true; /* all editing is enabled for DA windows */
    cutCopyClear = true;
    paste = true;
  } else if (IsAppWindow(window)) {
    te = ((DocumentPeek)window)->docTE;
    if ((*te)->selStart < (*te)->selEnd) cutCopyClear = true;
    /* Cut, Copy, and Clear is enabled for app. windows with selections */
    if (GetScrap(nil, 'TEXT', &offset) > 0)
      paste = true; /* if there's any text in the clipboard, paste is enabled */
  }
  if (undo)
    EnableItem(menu, iUndo);
  else
    DisableItem(menu, iUndo);
  if (cutCopyClear) {
    EnableItem(menu, iCut);
    EnableItem(menu, iCopy);
    EnableItem(menu, iClear);
  } else {
    DisableItem(menu, iCut);
    DisableItem(menu, iCopy);
    DisableItem(menu, iClear);
  }
  if (paste)
    EnableItem(menu, iPaste);
  else
    DisableItem(menu, iPaste);
} /*AdjustMenus*/

#if !UNIVERSAL_INTERFACE
Ptr somePtr;
Handle someHndl = &somePtr;

// Adapted from
// https://github.com/autc04/executor/blob/27c8ef28bc0ea29e7621466068c4e30aea664562/src/textedit/teScrap.cpp#L17-L30
pascal OSErr TEFromScrap() {
  long unused_offset;
  long len;
  Handle sh = LMGetTEScrpHandle();
  len = GetScrap(sh, 'TEXT', &unused_offset);
  if (len < 0) {
    EmptyHandle(sh);
    LMSetTEScrpLength(0);
    return len;
  } else {
    LMSetTEScrpLength(len);
    return noErr;
  }
}

pascal OSErr TEToScrap() {
  PScrapStuff scrapInfo = InfoScrap();
  // XXX how detect whether scrap hasn't been initialized?
  // Is checking for length = 0 enough?
  // should return noScrapErr = -100 in that case
  long scrapLength = LMGetTEScrpLength();
  if (scrapLength == 0) {
    return noScrapErr;
  }
  Handle h = LMGetTEScrpHandle();
  return PutScrap(scrapLength, 'TEXT', *h);
}

pascal long TEGetScrapLength() { return LMGetTEScrpLength(); }
#endif

/*	This is called when an item is chosen from the menu bar (after calling
        MenuSelect or MenuKey). It does the right thing for each command. */

void DoMenuCommand(long menuResult) {
  short menuID, menuItem;
  short itemHit, daRefNum;
  Str255 daName;
  OSErr saveErr;
  TEHandle te;
  WindowPtr window;
  Handle aHandle;
  long oldSize, newSize;
  long total, contig;

  window = FrontWindow();
  menuID = HiWord(menuResult);   /* use macros for efficiency to... */
  menuItem = LoWord(menuResult); /* get menu item number and menu number */
  switch (menuID) {
    case mApple:
      switch (menuItem) {
        case iAbout: /* bring up alert for About */
          itemHit = Alert(rAboutAlert, nil);
          break;
        default: /* all non-About items in this menu are DAs et al */
          /* type Str255 is an array in MPW 3 */
          GetMenuItemText(GetMenuHandle(mApple), menuItem, daName);
          daRefNum = OpenDeskAcc(daName);
          break;
      }
      break;
    case mFile:
      switch (menuItem) {
        case iNew:
          DoNew();
          break;
        case iClose:
          DoCloseWindow(FrontWindow()); /* ignore the result */
          break;
        case iQuit:
          Terminate();
          break;
      }
      break;
    case mEdit: /* call SystemEdit for DA editing & MultiFinder */
      if (!SystemEdit(menuItem - 1)) {
        te = ((DocumentPeek)FrontWindow())->docTE;
        switch (menuItem) {
          case iCut:
            if (ZeroScrap() == noErr) {
              PurgeSpace(&total, &contig);
              if ((*te)->selEnd - (*te)->selStart + kTESlop > contig)
                AlertUser(eNoSpaceCut);
              else {
                TECut(te);
                if (TEToScrap() != noErr) {
                  AlertUser(eNoCut);
                  ZeroScrap();
                }
              }
            }
            break;
          case iCopy:
            if (ZeroScrap() == noErr) {
              TECopy(te); /* after copying, export the TE scrap */
              if (TEToScrap() != noErr) {
                AlertUser(eNoCopy);
                ZeroScrap();
              }
            }
            break;
          case iPaste: /* import the TE scrap before pasting */
            if (TEFromScrap() == noErr) {
              if (TEGetScrapLength() +
                      ((*te)->teLength - ((*te)->selEnd - (*te)->selStart)) >
                  kMaxTELength)
                AlertUser(eExceedPaste);
              else {
                aHandle = (Handle)TEGetText(te);
                oldSize = GetHandleSize(aHandle);
                newSize = oldSize + TEGetScrapLength() + kTESlop;
                SetHandleSize(aHandle, newSize);
                saveErr = MemError();
                SetHandleSize(aHandle, oldSize);
                if (saveErr != noErr)
                  AlertUser(eNoSpacePaste);
                else
                  TEPaste(te);
              }
            } else
              AlertUser(eNoPaste);
            break;
          case iClear:
            TEDelete(te);
            break;
        }
        AdjustScrollbars(window, false);
        AdjustTE(window);
      }
      break;
  }
  HiliteMenu(0); /* unhighlight what MenuSelect (or MenuKey) hilited */
} /*DoMenuCommand*/

/* Create a new document and window. */

void DoNew() {
  Boolean good;
  Ptr storage;
  WindowPtr window;
  Rect destRect, viewRect;
  DocumentPeek doc;

  storage = NewPtr(sizeof(DocumentRecord));
  if (storage != nil) {
    window = GetNewWindow(rDocWindow, storage, (WindowPtr)-1);
    if (window != nil) {
      gNumDocuments +=
          1; /* this will be decremented when we call DoCloseWindow */
      good = false;
      SetPort(window);
      doc = (DocumentPeek)window;
      GetTERect(window, &viewRect);
      destRect = viewRect;
      destRect.right = destRect.left + kMaxDocWidth;
      doc->docTE = TENew(&destRect, &viewRect);
      good =
          doc->docTE != nil; /* if TENew succeeded, we have a good document */
      if (good) {            /* 1.02 - good document? -- proceed */
        AdjustViewRect(doc->docTE);
        TEAutoView(true, doc->docTE);
        doc->docClick = (*doc->docTE)->clikLoop;
        (*doc->docTE)->clikLoop = (ProcPtr)AsmClikLoop;
      }

      if (good) { /* good document? -- get scrollbars */
        doc->docVScroll = GetNewControl(rVScroll, window);
        good = (doc->docVScroll != nil);
      }
      if (good) {
        doc->docHScroll = GetNewControl(rHScroll, window);
        good = (doc->docHScroll != nil);
      }

      if (good) { /* good? -- adjust & draw the controls, draw the window */
        /* false to AdjustScrollValues means musn't redraw; technically, of
        course, the window is hidden so it wouldn't matter whether we called
        ShowControl or not. */
        AdjustScrollValues(window, false);
        ShowWindow(window);
      } else {
        DoCloseWindow(window); /* otherwise regret we ever created it... */
        AlertUser(eNoWindow);  /* and tell user */
      }
    } else
      DisposePtr(storage); /* get rid of the storage if it is never used */
  }
} /*DoNew*/

/* Close a window. This handles desk accessory and application windows. */

/*	1.01 - At this point, if there was a document associated with a
        window, you could do any document saving processing if it is 'dirty'.
        DoCloseWindow would return true if the window actually closed, i.e.,
        the user didn't cancel from a save dialog. This result is handy when
        the user quits an application, but then cancels the save of a document
        associated with a window. */

Boolean DoCloseWindow(WindowPtr window) {
  TEHandle te;

  if (IsDAWindow(window))
    CloseDeskAcc(((WindowPeek)window)->windowKind);
  else if (IsAppWindow(window)) {
    te = ((DocumentPeek)window)->docTE;
    if (te != nil)
      TEDispose(te); /* dispose the TEHandle if we got far enough to make one */
    /*	1.01 - We used to call DisposeWindow, but that was technically
            incorrect, even though we allocated storage for the window on
            the heap. We should instead call CloseWindow to have the structures
            taken care of and then dispose of the storage ourselves. */
    CloseWindow(window);
    DisposePtr((Ptr)window);
    gNumDocuments -= 1;
  }
  return true;
} /*DoCloseWindow*/

/**************************************************************************************
*** 1.01 DoCloseBehind(window) was removed ***

        1.01 - DoCloseBehind was a good idea for closing windows when quitting
        and not having to worry about updating the windows, but it suffered
        from a fatal flaw. If a desk accessory owned two windows, it would
        close both those windows when CloseDeskAcc was called. When
DoCloseBehind got around to calling DoCloseWindow for that other window that was
already closed, things would go very poorly. Another option would be to have a
        procedure, GetRearWindow, that would go through the window list and
return the last window. Instead, we decided to present the standard approach of
getting and closing FrontWindow until FrontWindow returns NIL. This has a
potential benefit in that the window whose document needs to be saved may be
visible since it is the front window, therefore decreasing the chance of user
confusion. For aesthetic reasons, the windows in the application should be
checked for updates periodically and have the updates serviced.
**************************************************************************************/

/* Clean up the application and exit. We close all of the windows so that
 they can update their documents, if any. */

/*	1.01 - If we find out that a cancel has occurred, we won't exit to the
        shell, but will return instead. */

void Terminate() {
  WindowPtr aWindow;
  Boolean closed;

  closed = true;
  do {
    aWindow = FrontWindow(); /* get the current front window */
    if (aWindow != nil) closed = DoCloseWindow(aWindow); /* close this window */
  } while (closed && (aWindow != nil));
  if (closed) ExitToShell(); /* exit if no cancellation */
} /*Terminate*/

/* Gets called from our assembly language routine, AsmClickLoop, which is in
        turn called by the TEClick toolbox routine. Saves the windows clip
   region, sets it to the portRect, adjusts the scrollbar values to match the TE
   scroll amount, then restores the clip region. */

Boolean IsAppWindow(WindowPtr window) {
  short windowKind;

  if (window == nil)
    return false;
  else { /* application windows have windowKinds = userKind (8) */
    windowKind = ((WindowPeek)window)->windowKind;
    return (windowKind == userKind);
  }
} /*IsAppWindow*/

/* Check to see if a window belongs to a desk accessory. */

Boolean IsDAWindow(WindowPtr window) {
  if (window == nil)
    return false;
  else /* DA windows have negative windowKinds */
    return ((WindowPeek)window)->windowKind < 0;
} /*IsDAWindow*/

/*	Display an alert that tells the user an error occurred, then exit the
   program. This routine is used as an ultimate bail-out for serious errors that
   prohibit the continuation of the application. Errors that do not require the
   termination of the application should be handled in a different manner. Error
   checking and reporting has a place even in the simplest application. The
   error number is used to index an 'STR#' resource so that a relevant message
   can be displayed. */

void AlertUser(short error) {
  short itemHit;
  Str255 message;

  SetCursor(&qd.arrow);
  /* type Str255 is an array in MPW 3 */
  GetIndString(message, kErrStrings, error);
  ParamText(message, "", "", "");
  itemHit = Alert(rUserAlert, nil);
} /* AlertUser */
