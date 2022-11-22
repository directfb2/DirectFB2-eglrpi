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

#include <core/core.h>
#include <core/core_system.h>
#include <core/layers.h>
#include <core/screens.h>
#include <core/surface_pool.h>
#include <fusion/shmalloc.h>

#include "egl_system.h"

D_DEBUG_DOMAIN( EGL_System, "EGL/System", "EGL System Module" );

DFB_CORE_SYSTEM( eglrpi )

/**********************************************************************************************************************/

extern const ScreenFuncs       eglScreenFuncs;
extern const DisplayLayerFuncs eglPrimaryLayerFuncs;
extern const SurfacePoolFuncs  eglSurfacePoolFuncs;

static DFBResult
local_init( EGLData *egl )
{
     CoreScreen               *screen;
     EGLConfig                 config;
     EGLint                    num_config;
     DISPMANX_MODEINFO_T       modeinfo;
     DISPMANX_UPDATE_HANDLE_T  update_handle;
     VC_RECT_T                 dst_rect;
     VC_RECT_T                 src_rect;
     const EGLint              config_attr[]  = { EGL_RED_SIZE,   8,
                                                  EGL_GREEN_SIZE, 8,
                                                  EGL_BLUE_SIZE,  8,
                                                  EGL_ALPHA_SIZE, 8,
                                                  EGL_NONE };
     const EGLint              context_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2,
                                                  EGL_NONE };

     /* Open EGL display. */
     bcm_host_init();

     egl->dispmanx_display = vc_dispmanx_display_open( DISPMANX_ID_MAIN_LCD );
     if (egl->dispmanx_display == DISPMANX_NO_HANDLE) {
          D_ERROR( "EGL/System: vc_dispmanx_display_open() failed!\n" );
          return DFB_INIT;
     }

     egl->eglDisplay = eglGetDisplay( EGL_DEFAULT_DISPLAY );
     if (!egl->eglDisplay) {
          D_ERROR( "EGL/System: eglGetDisplay() failed: 0x%x!\n", (unsigned int) eglGetError() );
          return DFB_INIT;
     }

     if (!eglInitialize( egl->eglDisplay, NULL, NULL )) {
          D_ERROR( "EGL/System: eglInitialize() failed: 0x%x!\n", (unsigned int) eglGetError() );
          return DFB_INIT;
     }

     if (!eglChooseConfig( egl->eglDisplay, config_attr, &config, 1, &num_config ) || (num_config != 1)) {
          D_ERROR("DirectFB/EGL: eglChooseConfig() failed: 0x%x!\n", (unsigned int) eglGetError() );
          return DFB_INIT;
     }

     /* Retrieve display information. */
     if (vc_dispmanx_display_get_info( egl->dispmanx_display, &modeinfo ) < 0) {
           D_ERROR( "EGL/System: vc_dispmanx_display_get_info() failed!\n" );
           return DFB_INIT;
     }

     egl->size.w = modeinfo.width;
     egl->size.h = modeinfo.height;

     D_INFO( "EGL/System: Found display configuration\n" );

     /* Create EGL window surface. */
     update_handle = vc_dispmanx_update_start( 0 );
     if (update_handle == DISPMANX_NO_HANDLE) {
          D_ERROR( "EGL/System: vc_dispmanx_update_start() failed!\n" );
          return DFB_INIT;
     }

     dst_rect.x      = 0;
     dst_rect.y      = 0;
     dst_rect.width  = egl->size.w;
     dst_rect.height = egl->size.h;

     src_rect.x      = 0;
     src_rect.y      = 0;
     src_rect.width  = egl->size.w << 16;
     src_rect.height = egl->size.h << 16;

     egl->dispmanx_element = vc_dispmanx_element_add( update_handle, egl->dispmanx_display, 0, &dst_rect,
                                                      DISPMANX_NO_HANDLE, &src_rect, DISPMANX_PROTECTION_NONE,
                                                      NULL, NULL, DISPMANX_NO_ROTATE );
     if (egl->dispmanx_element == DISPMANX_NO_HANDLE) {
          D_ERROR( "EGL/System: vc_dispmanx_element_add() failed!\n" );
          return DFB_INIT;
     }

     if (vc_dispmanx_update_submit_sync( update_handle ) < 0) {
          D_ERROR( "EGL/System: vc_dispmanx_update_submit_sync() failed!\n" );
          return DFB_INIT;
     }

     egl->dispmanx_window.element = egl->dispmanx_element;
     egl->dispmanx_window.width   = egl->size.w;
     egl->dispmanx_window.height  = egl->size.h;

     egl->eglSurface = eglCreateWindowSurface( egl->eglDisplay, config, (EGLNativeWindowType) &egl->dispmanx_window, NULL );
     if (!egl->eglSurface) {
          D_ERROR( "EGL/System: eglCreateWindowSurface() failed: 0x%x!\n", (unsigned int) eglGetError() );
          return DFB_INIT;
     }

     /* Create EGL context and attach it to the EGL window surface. */
     egl->eglContext = eglCreateContext( egl->eglDisplay, config, EGL_NO_CONTEXT, context_attr );
     if (!egl->eglContext) {
          D_ERROR( "EGL/System: eglCreateContext() failed: 0x%x!\n", (unsigned int) eglGetError() );
          return DFB_INIT;
     }

     if (!eglMakeCurrent( egl->eglDisplay, egl->eglSurface, egl->eglSurface, egl->eglContext )) {
          D_ERROR( "EGL/System: eglMakeCurrent() failed: 0x%x!\n", (unsigned int) eglGetError() );
          return DFB_INIT;
     }

     screen = dfb_screens_register( egl, &eglScreenFuncs );

     dfb_layers_register( screen, egl, &eglPrimaryLayerFuncs );

     return DFB_OK;
}

static DFBResult
local_deinit( EGLData *egl )
{
     if (egl->eglContext) {
          eglMakeCurrent( egl->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
          eglDestroyContext( egl->eglDisplay, egl->eglContext );
     }

     if (egl->eglSurface)
          eglDestroySurface( egl->eglDisplay, egl->eglSurface );

     if (egl->dispmanx_element != DISPMANX_NO_HANDLE) {
          DISPMANX_UPDATE_HANDLE_T update_handle;

          if ((update_handle = vc_dispmanx_update_start( 0 )) != DISPMANX_NO_HANDLE) {
               vc_dispmanx_element_remove( update_handle, egl->dispmanx_element );
               vc_dispmanx_update_submit_sync( update_handle );
          }
     }

     if (egl->eglDisplay)
          eglTerminate( egl->eglDisplay );

     if (egl->dispmanx_display != DISPMANX_NO_HANDLE)
          vc_dispmanx_display_close( egl->dispmanx_display );

     bcm_host_deinit();

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->version.major = 0;
     info->version.minor = 1;

     info->caps = CSCAPS_ACCELERATION | CSCAPS_ALWAYS_INDIRECT;

     snprintf( info->name,   DFB_CORE_SYSTEM_INFO_NAME_LENGTH,   "EGL" );
     snprintf( info->vendor, DFB_CORE_SYSTEM_INFO_VENDOR_LENGTH, "DirectFB" );
}

static DFBResult
system_initialize( CoreDFB  *core,
                   void    **ret_data )
{
     DFBResult            ret;
     EGLData             *egl;
     EGLDataShared       *shared;
     FusionSHMPoolShared *pool;

     D_DEBUG_AT( EGL_System, "%s()\n", __FUNCTION__ );

     egl = D_CALLOC( 1, sizeof(EGLData) );
     if (!egl)
          return D_OOM();

     egl->core = core;

     pool = dfb_core_shmpool( core );

     shared = SHCALLOC( pool, 1, sizeof(EGLDataShared) );
     if (!shared) {
          D_FREE( egl );
          return D_OOSHM();
     }

     shared->shmpool = pool;

     egl->shared = shared;

     ret = local_init( egl );
     if (ret)
          goto error;

     *ret_data = egl;

     ret = dfb_surface_pool_initialize( core, &eglSurfacePoolFuncs, &shared->pool );
     if (ret)
          goto error;

     ret = core_arena_add_shared_field( core, "egl", shared );
     if (ret)
          goto error;

     return DFB_OK;

error:
     local_deinit( egl );

     SHFREE( pool, shared );

     D_FREE( egl );

     return ret;
}

static DFBResult
system_join( CoreDFB  *core,
             void    **ret_data )
{
     DFBResult      ret;
     EGLData       *egl;
     EGLDataShared *shared;

     D_DEBUG_AT( EGL_System, "%s()\n", __FUNCTION__ );

     egl = D_CALLOC( 1, sizeof(EGLData) );
     if (!egl)
          return D_OOM();

     egl->core = core;

     ret = core_arena_get_shared_field( core, "egl", (void**) &shared );
     if (ret) {
          D_FREE( egl );
          return ret;
     }

     egl->shared = shared;

     ret = local_init( egl );
     if (ret)
          goto error;

     *ret_data = egl;

     ret = dfb_surface_pool_join( core, shared->pool, &eglSurfacePoolFuncs );
     if (ret)
          goto error;

     return DFB_OK;

error:
     local_deinit( egl );

     D_FREE( egl );

     return ret;
}

static DFBResult
system_shutdown( bool emergency )
{
     EGLData       *egl = dfb_system_data();
     EGLDataShared *shared;

     D_DEBUG_AT( EGL_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( egl != NULL );
     D_ASSERT( egl->shared != NULL );

     shared = egl->shared;

     dfb_surface_pool_destroy( shared->pool );

     local_deinit( egl );

     SHFREE( shared->shmpool, shared );

     D_FREE( egl );

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     EGLData       *egl = dfb_system_data();
     EGLDataShared *shared;

     D_DEBUG_AT( EGL_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( egl != NULL );
     D_ASSERT( egl->shared != NULL );

     shared = egl->shared;

     dfb_surface_pool_leave( shared->pool );

     local_deinit( egl );

     D_FREE( egl );

     return DFB_OK;
}

static DFBResult
system_suspend()
{
     return DFB_OK;
}

static DFBResult
system_resume()
{
     return DFB_OK;
}

static VideoMode *
system_get_modes()
{
     return NULL;
}

static VideoMode *
system_get_current_mode()
{
     return NULL;
}

static DFBResult
system_thread_init()
{
     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     return false;
}

static volatile void *
system_map_mmio( unsigned int offset,
                 int          length )
{
     return NULL;
}

static void
system_unmap_mmio( volatile void *addr,
                   int            length )
{
}

static int
system_get_accelerator()
{
     return direct_config_get_int_value( "accelerator" );
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_videoram_length()
{
     return 0;
}

static void
system_get_busid( int *ret_bus,
                  int *ret_dev,
                  int *ret_func )
{
     return;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     return;
}
