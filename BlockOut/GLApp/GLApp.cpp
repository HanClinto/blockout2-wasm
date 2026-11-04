// -------------------------------------------
// SDL/OpenGL OpenGL application framework
// Jean-Luc PONS (2007)
// -------------------------------------------
#include "GLApp.h"
#ifndef WINDOWS
#include <unistd.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#undef LoadImage
#include <CImage.h>

extern char *LID(char *fileName);

// -------------------------------------------

GLApplication::GLApplication() {

  m_bWindowed = true;
  m_bVSync = false;
  m_strWindowTitle = (char *)"GL application";
  strcpy((char *)m_strFrameStats,"");
  m_screenWidth = 640;
  m_screenHeight = 480;

}

// -------------------------------------------

int GLApplication::SetVideoMode() {

  // Enable/Disable vertical blanking 
  // Work only at application startup
#ifndef __EMSCRIPTEN__
  SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL,m_bVSync);
#endif

  // Set the video mode
  Uint32 flags;
  if( m_bWindowed ) flags = SDL_DOUBLEBUF | SDL_OPENGL;
  else              flags = SDL_DOUBLEBUF | SDL_OPENGL | SDL_FULLSCREEN;

  if( SDL_SetVideoMode( m_screenWidth, m_screenHeight, 0, flags ) == NULL )
  {
#ifdef WINDOWS
    char message[256];
	sprintf(message,"SDL_SetVideoMode() failed : %s\n",SDL_GetError());
	MessageBox(NULL,message,"ERROR",MB_OK|MB_ICONERROR);
#else
    printf("SDL_SetVideoMode() failed : %s\n",SDL_GetError());
#endif
    return GL_FAIL;
  }

  if( !m_bWindowed )
    SDL_ShowCursor(SDL_DISABLE); 
  else
    SDL_ShowCursor(SDL_ENABLE); 

  return GL_OK;

}

// -------------------------------------------

int GLApplication::ToggleFullscreen() {

  int errCode;

  InvalidateDeviceObjects();

  m_bWindowed = !m_bWindowed;

  if( !SetVideoMode() ) return GL_FAIL;

  SDL_Surface *vSurf = SDL_GetVideoSurface();
  m_bitsPerPixel = vSurf->format->BitsPerPixel;

  errCode = RestoreDeviceObjects();
  if( !errCode ) {
    printGlError();
    exit(1);
  }

  return GL_OK;
    
}

// -------------------------------------------

int GLApplication::Create(int width, int height, BOOL bFullScreen, BOOL bVSync ) {

  int errCode;

  m_bVSync = bVSync;
  m_screenWidth = width;
  m_screenHeight = height;
  m_bWindowed = !bFullScreen;
  
#if !defined(WINDOWS) && !defined(__EMSCRIPTEN__)
  if( getenv("DISPLAY")==NULL ) {
    printf("Warning, DISPLAY not defined, it may not work.\n");
  }
#endif

  //Initialize SDL
  if( SDL_Init( SDL_INIT_EVERYTHING ) < 0 )
  {
#ifdef WINDOWS
    char message[256];
	sprintf(message,"SDL_Init() failed : %s\n" , SDL_GetError() );
	MessageBox(NULL,message,"Error",MB_OK|MB_ICONERROR);
#else
    printf("SDL_Init() failed : %s\n" , SDL_GetError() );
#endif
	return GL_FAIL;    
  }

  SDL_WM_SetCaption(m_strWindowTitle, NULL);

  //SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0);
  SDL_EnableUNICODE( 1 );
  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);
    
  //Create Window
  if( !SetVideoMode() ) return GL_FAIL;

  SDL_Surface *vSurf = SDL_GetVideoSurface();
  m_bitsPerPixel = vSurf->format->BitsPerPixel;

  OneTimeSceneInit();
  errCode = RestoreDeviceObjects();
  if( !errCode ) {
    printGlError();
    exit(0);
  }

  return GL_OK;

}

// -------------------------------------------

void GLApplication::Pause(BOOL bPause) {
}

// -------------------------------------------

int GLApplication::Resize( DWORD width, DWORD height ) {

  int errCode;
  m_screenWidth = width;
  m_screenHeight = height;

  InvalidateDeviceObjects();

  if( !SetVideoMode() ) return GL_FAIL;

  SDL_Surface *vSurf = SDL_GetVideoSurface();
  m_bitsPerPixel = vSurf->format->BitsPerPixel;

  errCode = RestoreDeviceObjects();
  if( !errCode ) {
    printGlError();
    exit(1);
  }

  return GL_OK;

}

// -------------------------------------------

#ifdef __EMSCRIPTEN__
// The browser calls this once per frame (requestAnimationFrame) in place of Run()'s loop
static void StepFrame(void *app) {
  if( !((GLApplication *)app)->Step() ) emscripten_cancel_main_loop();
}

// A GL error ends the native game; in the browser it's reported (the first few) and play goes on
static bool GLErrorFatal(GLenum err) {
  static int reported = 0;
  if( reported < 10 ) {
    printf("OpenGL error 0x%04X\n", err);
    reported++;
  }
  return false;
}
#else
static bool GLErrorFatal(GLenum err) {
  GLApplication::printGlError();
  return true;
}
#endif

int GLApplication::Run() {

  m_fTime        = 0.0f;
  m_fElapsedTime = 0.0f;
  m_fFPS         = 0.0f;
  nbFrame = 0;
  lastTick = lastFrTick = firstTick = SDL_GetTicks();

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(StepFrame, this, 0, 1);
  return GL_OK;
#endif

  //Wait for user exit
  while( Step() );

  //Clean up
  SDL_Quit();

  return GL_OK;

}

bool GLApplication::Step() {

  SDL_Event event;
  bool quit = false;
  int  errCode = GL_OK;
  int  fTick;
  GLenum glErr;

     //While there are events to handle
     while( SDL_PollEvent( &event ) ) {
       if( event.type == SDL_QUIT )
         quit = true;
       else
         EventProc(&event);
     }

     fTick = SDL_GetTicks();

     // Update timing
     nbFrame++;
     if( (fTick - lastTick) >= 1000 ) {
        int t0 = fTick;
        int t = t0 - lastTick;
        m_fFPS = (float)(nbFrame*1000) / (float)t;
        nbFrame = 0;
        lastTick = t0;
        sprintf(m_strFrameStats,"%.2f fps (%dx%dx%d)",m_fFPS,m_screenWidth,m_screenHeight,m_bitsPerPixel);
     }

     m_fTime = (float) ( fTick - firstTick ) / 1000.0f;
     m_fElapsedTime = (fTick - lastFrTick) / 1000.0f;
     lastFrTick = fTick;

     if(!quit) errCode = FrameMove();
     if( !errCode ) quit = true;
     if( (glErr = glGetError()) != GL_NO_ERROR && GLErrorFatal(glErr) ) quit = true;

     if(!quit) errCode = Render();
     if( !errCode ) quit = true;
     if( (glErr = glGetError()) != GL_NO_ERROR && GLErrorFatal(glErr) ) quit = true;

     //Swap buffer
     SDL_GL_SwapBuffers();

  return !quit;

}

// -------------------------------------------

void GLApplication::SetMaterial(GLMATERIAL *mat) {

  float acolor[] = { mat->Ambient.r, mat->Ambient.g, mat->Ambient.b, mat->Ambient.a };
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, acolor);
  float dcolor[] = { mat->Diffuse.r, mat->Diffuse.g, mat->Diffuse.b, mat->Diffuse.a };
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, dcolor);
  float scolor[] = { mat->Specular.r, mat->Specular.g, mat->Specular.b, mat->Specular.a };
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, scolor);
  float ecolor[] = { mat->Emissive.r, mat->Emissive.g, mat->Emissive.b, mat->Emissive.a };
  glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, ecolor);
  glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat->Power);
  glColor4f(mat->Ambient.r, mat->Ambient.g, mat->Ambient.b, mat->Ambient.a);

}

// -------------------------------------------

void GLApplication::printGlError() {
  
  char message[256];

  GLenum errCode = glGetError();
  switch( errCode ) {
    case GL_INVALID_ENUM:
      sprintf(message,"OpenGL failure: An unacceptable value is specified for an enumerated argument. The offending function is ignored, having no side effect other than to set the error flag.");
      break;
    case GL_INVALID_VALUE:
	  sprintf(message, "OpenGL failure: A numeric argument is out of range. The offending function is ignored, having no side effect other than to set the error flag.");
      break;
    case GL_INVALID_OPERATION:
	  sprintf(message, "OpenGL failure: The specified operation is not allowed in the current state. The offending function is ignored, having no side effect other than to set the error flag.");
      break;
    case GL_STACK_OVERFLOW:
	  sprintf(message, "OpenGL failure: This function would cause a stack overflow. The offending function is ignored, having no side effect other than to set the error flag.");
      break;
    case GL_STACK_UNDERFLOW:
	  sprintf(message, "OpenGL failure: This function would cause a stack underflow. The offending function is ignored, having no side effect other than to set the error flag.");
      break;
    case GL_OUT_OF_MEMORY:
      sprintf(message, "OpenGL failure: There is not enough memory left to execute the function. The state of OpenGL is undefined, except for the state of the error flags, after this error is recorded.");
      break;
	default:
      sprintf(message, "Application failure.");
	  break;
  }

#ifdef WINDOWS
  MessageBox(NULL, message, "Error", MB_OK | MB_ICONERROR);
#else
  printf(message);
#endif

}

