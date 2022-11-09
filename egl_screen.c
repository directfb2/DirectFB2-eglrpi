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

#include <core/screens.h>
#include <misc/conf.h>

#include "egl_system.h"

D_DEBUG_DOMAIN( EGL_Screen, "EGL/Screen", "EGL Screen" );

/**********************************************************************************************************************/

static int hor[] = {
      640,  720,  720,  800, 1024, 1152, 1280, 1280, 1280, 1280, 1400, 1600, 1920, 960, 1440, 800, 1024, 1366, 1920,
     2560, 2560, 3840, 4096
};

static int ver[] = {
      480,  480,  576,  600,  768,  864,  720,  768,  960, 1024, 1050, 1200, 1080, 540,  540, 480,  600,  768, 1200,
     1440, 1600, 2160, 2160
};

static DFBResult
eglInitScreen( CoreScreen           *screen,
               void                 *driver_data,
               void                 *screen_data,
               DFBScreenDescription *description )
{
     EGLData       *egl = driver_data;
     EGLDataShared *shared;

     D_DEBUG_AT( EGL_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( egl != NULL );
     D_ASSERT( egl->shared != NULL );

     shared = egl->shared;

     /* Set capabilities. */
     description->caps    = DSCCAPS_OUTPUTS;
     description->outputs = 1;

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH, "EGL Screen" );

     shared->mode.w = dfb_config->mode.width  ?: egl->size.w;
     shared->mode.h = dfb_config->mode.height ?: egl->size.h;

     return DFB_OK;
}

static DFBResult
eglInitOutput( CoreScreen                 *screen,
               void                       *driver_data,
               void                       *screen_data,
               int                         output,
               DFBScreenOutputDescription *description,
               DFBScreenOutputConfig      *config )
{
     EGLData       *egl = driver_data;
     EGLDataShared *shared;
     int            j;

     D_DEBUG_AT( EGL_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( egl != NULL );
     D_ASSERT( egl->shared != NULL );

     shared = egl->shared;

     /* Set capabilities. */
     description->caps = DSOCAPS_RESOLUTION;

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_OUTPUT_DESC_NAME_LENGTH, "EGL Output" );

     config->flags = DSOCONF_RESOLUTION;

     config->resolution = DSOR_UNKNOWN;

     for (j = 0; j < D_ARRAY_SIZE(hor); j++) {
          if (shared->mode.w == hor[j] && shared->mode.h == ver[j]) {
               config->resolution = 1 << j;
               break;
          }
     }

     return DFB_OK;
}

static DFBResult
eglSetOutputConfig( CoreScreen                  *screen,
                    void                        *driver_data,
                    void                        *screen_data,
                    int                          output,
                    const DFBScreenOutputConfig *config )
{
     EGLData       *egl    = driver_data;
     EGLDataShared *shared = egl->shared;
     int            res;

     D_DEBUG_AT( EGL_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( egl != NULL );
     D_ASSERT( egl->shared != NULL );

     if (config->flags != DSOCONF_RESOLUTION)
          return DFB_INVARG;

     res = D_BITn32( config->resolution );
     if (res == -1 || res >= D_ARRAY_SIZE(hor))
          return DFB_INVARG;

     shared->mode.w = hor[res];
     shared->mode.h = ver[res];

     return DFB_OK;
}

static DFBResult
eglGetScreenSize( CoreScreen *screen,
                  void       *driver_data,
                  void       *screen_data,
                  int        *ret_width,
                  int        *ret_height )
{
     EGLData       *egl = driver_data;
     EGLDataShared *shared;

     D_DEBUG_AT( EGL_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( egl != NULL );
     D_ASSERT( egl->shared != NULL );

     shared = egl->shared;

     *ret_width  = shared->mode.w;
     *ret_height = shared->mode.h;

     return DFB_OK;
}

const ScreenFuncs eglScreenFuncs = {
     .InitScreen      = eglInitScreen,
     .InitOutput      = eglInitOutput,
     .SetOutputConfig = eglSetOutputConfig,
     .GetScreenSize   = eglGetScreenSize
};
