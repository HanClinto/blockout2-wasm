/*
  File:        BlockOut.cpp
  Description: Main application class
  Program:     BlockOut
  Author:      Jean-Luc PONS

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#include "BlockOut.h"

#ifndef WINDOWS
#include <unistd.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "../web/webhost.h"
static class BlockOut *theApp = NULL;  // for the page's calls (web_pause)
#endif
#else
extern void InitialiseWinsock();
#endif

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program. Initializes everything, and goes into a
//       message-processing loop. Idle time is used to render the scene.
//-----------------------------------------------------------------------------
#ifdef WINDOWS
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nCmdShow)
#else
int main(int argc,char *argv[])
#endif
{

  // Check environement
  if( !CheckEnv() ) {
    return 0;
  }

#ifdef WINDOWS
  InitialiseWinsock();
#endif
    
  // Create and start the application
  BlockOut *glApp = new BlockOut();
#ifdef __EMSCRIPTEN__
  // A page always opens in a window; the page's own button (or F1) goes full screen. The page picks the resolution
  // that fits its screen.
  glApp->theSetup.SetFullScreen(FALSE);
  glApp->theSetup.SetWindowSize(WebWindowSize(glApp->theSetup.GetWindowSize()));
  theApp = glApp;
#endif
    
  if (!glApp->Create(glApp->theSetup.GetWindowWidth(),
	  glApp->theSetup.GetWindowHeight(),
	  glApp->theSetup.GetFullScreen(),
	  glApp->theSetup.GetFrLimiter() == FR_LIMITVSYNC)) {
	  delete glApp;
	  return 0;
  }

  return glApp->Run();
}

#ifdef __EMSCRIPTEN__
//-----------------------------------------------------------------------------
// Name: web_pause()
// Desc: The page pauses a game in play (when its tab is hidden), as P does.
//-----------------------------------------------------------------------------
extern "C" EMSCRIPTEN_KEEPALIVE void web_pause()
{
  if( theApp ) theApp->PauseGame();
}

void BlockOut::PauseGame()
{
  if( mode==GAME_MODE && theGame.IsPlaying() ) m_bKey['p'] = 1;
}
#endif

//-----------------------------------------------------------------------------
// Name: BlockOut()
// Desc: Application constructor. Sets attributes for the app.
//-----------------------------------------------------------------------------
BlockOut::BlockOut()
{
  inited = FALSE;
  mode = MENU_MODE;
#ifdef __EMSCRIPTEN__
  gameReported = TRUE;
#endif
  m_strWindowTitle = STR(APP_VERSION);
  ZeroMemory( m_bKey, sizeof(m_bKey) );
}

//-----------------------------------------------------------------------------
// Name: UpdateFullScreen()
// Desc: Update fullscreen
//-----------------------------------------------------------------------------
int BlockOut::UpdateFullScreen()
{
  int hr = GL_OK;

#ifdef __EMSCRIPTEN__
  // The page goes full screen (the browser wants it from a key or click handler; the page waits for the next one)
  WebSetFullScreen(theSetup.GetFullScreen());
  return hr;
#endif

  if( m_bWindowed ) {
    if( theSetup.GetFullScreen() ) {
      // Go to fullscreen
      Pause(TRUE);
      hr = ToggleFullscreen();
      Pause(FALSE);
    }
  } else {
    if( !theSetup.GetFullScreen() ) {
      // Go to windowed
      Pause(TRUE);
      hr = ToggleFullscreen();
      Pause(FALSE);
    }
  }

  return hr;

}

//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
int BlockOut::FrameMove()
{

  // General keys

  if( m_bKey[SDLK_F1] ) {
    BOOL fs = theSetup.GetFullScreen();
    theSetup.SetFullScreen(!fs);
    UpdateFullScreen();
    m_bKey[SDLK_F1]=0;
  }

  // Processing
  int retValue;
  switch(mode) {
    case MENU_MODE:
      retValue = theMenu.Process(m_bKey,m_fTime);
      switch( retValue ) {
        case 1: // Switch to game mode
          ZeroMemory( m_bKey, sizeof(m_bKey) );
          theGame.StartGame(m_screenWidth,m_screenHeight,m_fTime);
#ifdef __EMSCRIPTEN__
          gameReported = FALSE;
          WebGameStarted();
#endif
          mode = GAME_MODE;
          break;
        case 2: // Resize
          Resize(theSetup.GetWindowWidth() , theSetup.GetWindowHeight());
          break;
        case 3:
          UpdateFullScreen();
          break;
        case 7:
          theGame.StartDemo(m_screenWidth,m_screenHeight,m_fTime);
          mode = GAME_MODE;
          break;
        case 8:
          theGame.StartPractice(m_screenWidth,m_screenHeight,m_fTime);
          mode = GAME_MODE;
          break;
        case 100: // Exit
#ifdef __EMSCRIPTEN__
          // Back to the page that opened the game; the game stays where it is if there's none
          WebQuit();
          break;
#endif
          InvalidateDeviceObjects();
          BOOL fs = theSetup.GetFullScreen();
          if (fs) {
            theSetup.SetFullScreen(!fs);
            UpdateFullScreen();
          }  
          _exit(0);
          break;
      }
      break;
    case GAME_MODE:

      // Frame limiter
#ifdef __EMSCRIPTEN__
      // The browser paces frames (and sleeping here would freeze the page)
      int toSleep = 0;
#else
      int toSleep = (int)(theSetup.GetFrLimitTime()*1000.0f);
#endif
      if( toSleep>0 ) {
        int elapsed = SDL_GetTicks() - lastSleepTime;
        toSleep -= elapsed;
#ifdef WINDOWS
        if(toSleep>0) Sleep(toSleep);
#else
        if(toSleep>0) usleep(toSleep*1000);
#endif
        lastSleepTime = SDL_GetTicks();
      }

      retValue = theGame.Process(m_bKey,m_fTime);
#ifdef __EMSCRIPTEN__
      // Told as the game ends (not when the player leaves the Game Over screen, which they might never do)
      if( theGame.IsCountedGameOver() && !gameReported ) {
        ReportGame();
        gameReported = TRUE;
      }
#endif

      switch( retValue ) {
        case 1: {
          // Check High Score
          SCOREREC *score = theGame.GetScore();
          SCOREREC *newScore;
          int pos = theSetup.InsertHighScore(score,&newScore);
          if( newScore ) {
            switch(theSetup.GetSoundType()) {
              case SOUND_BLOCKOUT2:
                theSound.PlayWellDone();
                break;
              case SOUND_BLOCKOUT:
                theSound.PlayWellDone2();
                break;
            }
          }
          theMenu.ToPage(&theMenu.hallOfFamePage,pos,(void *)newScore);
          // Switch to menu mode
          mode = MENU_MODE;
        }
        break;
        case 2: // Return from Demo
          ZeroMemory( m_bKey, sizeof(m_bKey) );
          mode = MENU_MODE;
          theMenu.FullRepaint();
          break;
      }
      break;
  }

  return GL_OK;
}

#ifdef __EMSCRIPTEN__
//-----------------------------------------------------------------------------
// Name: ReportGame()
// Desc: Tell the page about the game that just ended (web build): it sends it to
//       the score service. The setup id is the one BlockOut's online scores use.
//-----------------------------------------------------------------------------
void BlockOut::ReportGame()
{
  SCOREREC *s = theGame.GetScore();
  char json[512];
  sprintf(json,
    "{\"setupId\":%d,\"pit\":[%d,%d,%d],\"blockSet\":%d,\"score\":%d,\"cubes\":%d,"
    "\"lines\":[%d,%d,%d,%d,%d],\"emptyPits\":%d,\"startLevel\":%d,\"level\":%d,"
    "\"seconds\":%.1f,\"date\":%u}",
    theSetup.GetId(), theSetup.GetPitWidth(), theSetup.GetPitHeight(), theSetup.GetPitDepth(),
    theSetup.GetBlockSet(), s->score, s->nbCube, s->nbLine1, s->nbLine2, s->nbLine3, s->nbLine4,
    s->nbLine5, (int)s->emptyPit, s->startLevel, theGame.GetLevel(), s->gameTime, s->date);
  WebGameOver(json);
}
#endif

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
int BlockOut::Render()
{
    if(!inited) return GL_OK;

    // Begin the scene
    switch( mode ) {
      case MENU_MODE:
        theMenu.Render();
#ifdef _DEBUG
        // Output statistics
        m_pSmallFont.DrawText( 10,  10, m_strFrameStats );
#endif
        break;
      case GAME_MODE:
        theGame.Render();
#ifdef _DEBUG
        // Output statistics
        m_pSmallFont.DrawText( 200,  20, m_strFrameStats );
#endif
        break;
    }

    return GL_OK;
}

//-----------------------------------------------------------------------------
// Name: OneTimeSceneInit()
// Desc: Called during initial app startup, this function performs all the
//       permanent initialization.
//-----------------------------------------------------------------------------
int BlockOut::OneTimeSceneInit()
{

  if( !theSound.Create() ) {
#ifdef WINDOWS
    char message[256];
	sprintf(message,"Failed to initialize sound manager.\nNo sound will be played.\n%s\n",theSound.GetErrorMsg());
	MessageBox(NULL,message,"Warning",MB_OK|MB_ICONWARNING);
#else
    printf("Failed to initialize sound manager.\nNo sound will be played.\n%s\n",theSound.GetErrorMsg());
#endif
  }

  // Init default
  theSound.SetEnable(theSetup.GetSound());
  theHttp.SetProxy(theSetup.GetUseProxy());
  theHttp.SetProxyName(theSetup.GetProxyName());
  theHttp.SetProxyPort(theSetup.GetProxyPort());

  // Set manager
  theMenu.SetSetupManager(&theSetup);
  theMenu.SetSoundManager(&theSound);
  theMenu.SetHttp(&theHttp);
  theGame.SetSetupManager(&theSetup);
  theGame.SetSoundManager(&theSound);
  theGame.pFont = &m_pSmallFont;

  return GL_OK;
}

//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc: Initialize scene objects.
//-----------------------------------------------------------------------------
int BlockOut::RestoreDeviceObjects()
{
    GLfloat matView[16];

    m_pSmallFont.RestoreDeviceObjects(m_screenWidth,m_screenHeight);

    //Set clear color
    glClearColor( 0, 0, 0, 0 );
 
    // Set viewport
    glViewport(0,0,m_screenWidth,m_screenHeight);

    //Compute view matrix (Right Handed)
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    gluLookAt(0.0,0.0,0.0, 0.0,0.0,10.0, 0.0,1.0,0.0);
    glGetFloatv( GL_MODELVIEW_MATRIX , matView );

    // Light
    glShadeModel(GL_SMOOTH);

    GLfloat global_ambient[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);

    GLfloat ambientLight[]  = { 0.0f, 0.0f, 0.0f, 1.0f   };
    GLfloat diffuseLight[]  = { 1.0f, 1.0f, 1.0f, 1.0f   };
    GLfloat specularLight[] = { 1.0f, 1.0f, 1.0f, 1.0f   };
    GLfloat position[]      = { 15.0f, 10.0f, -10.0f, 1.0f };

    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glEnable(GL_LIGHT0);

    // Default for texure
    glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);

    // Default for zbuff
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    //If there was any errors
    if( glGetError() != GL_NO_ERROR )
    {
        return GL_FAIL;    
    }
    
    // Set up device objects
    if( !theMenu.Create(m_screenWidth,m_screenHeight) )
      return GL_FAIL;

    if( !theGame.Create(m_screenWidth,m_screenHeight) )
      return GL_FAIL;
  
    theGame.SetViewMatrix(matView);
    theMenu.SetViewMatrix(matView);

    inited = TRUE;
    return GL_OK;
}

//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
int BlockOut::InvalidateDeviceObjects()
{
    inited = FALSE;
    m_pSmallFont.InvalidateDeviceObjects();
    theGame.InvalidateDeviceObjects();
    theMenu.InvalidateDeviceObjects();

    return GL_OK;
}

//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: Message proc function to handle key and menu input
//-----------------------------------------------------------------------------
int BlockOut::EventProc(SDL_Event *event)
{

  /*
    case WM_PAINT:
    // Need a full repaint
    theGame.FullRepaint();
    theMenu.FullRepaint();
    break;
  */

  // Handle key presses
  if( event->type == SDL_KEYDOWN )
  {
    int unicode = (event->key.keysym.unicode & 0x7F);
#ifdef __EMSCRIPTEN__
    // Emscripten's SDL gives keys that aren't characters (arrows, Page Up...) their key code as the character, as
    // SDL on a desktop doesn't: ArrowLeft (80 | 1<<10) came through as 'P' and paused the game
    if( event->key.keysym.unicode >= 0x80 || (event->key.keysym.sym & SDLK_SCANCODE_MASK) ) unicode = 0;
#endif
    if( unicode ) {
      m_bKey[unicode] = 1;
    } else {
      m_bKey[event->key.keysym.sym] = 1;
    }
  }

  return GL_OK;

}
