/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#ifndef __EGL_SYSTEM_H__
#define __EGL_SYSTEM_H__

#include <bcm_host.h>
#include <core/coretypes.h>
#include <fusion/types.h>
#include <EGL/egl.h>

/**********************************************************************************************************************/

typedef struct {
     FusionSHMPoolShared *shmpool;

     CoreSurfacePool     *pool;

     DFBDimension         mode;    /* current video mode */
} EGLDataShared;

typedef struct {
     EGLDataShared             *shared;

     CoreDFB                   *core;

     DISPMANX_DISPLAY_HANDLE_T  dispmanx_display;
     EGLDisplay                 eglDisplay;

     DFBDimension               size;

     DISPMANX_ELEMENT_HANDLE_T  dispmanx_element;
     EGL_DISPMANX_WINDOW_T      dispmanx_window;

     EGLSurface                 eglSurface;
     EGLContext                 eglContext;
} EGLData;

#endif
