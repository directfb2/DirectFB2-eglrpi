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

#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "egl_system.h"

D_DEBUG_DOMAIN( EGL_Surfaces, "EGL/Surfaces", "EGL Surface Pool" );
D_DEBUG_DOMAIN( EGL_SurfLock, "EGL/SurfLock", "EGL Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int    magic;

     int    pitch;
     int    size;

     GLuint tex;
     GLuint fbo;
} EGLAllocationData;

/**********************************************************************************************************************/

static int
eglAllocationDataSize( void )
{
     return sizeof(EGLAllocationData);
}

static DFBResult
eglInitPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data,
             CoreSurfacePoolDescription *ret_desc )
{
     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_PHYSICAL | CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority          = CSPP_DEFAULT;

     /* For hardware layers. */
     ret_desc->access[CSAID_LAYER0] = CSAF_READ | CSAF_SHARED;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "EGL Surface Pool" );

     return DFB_OK;
}

static DFBResult
eglJoinPool( CoreDFB         *core,
             CoreSurfacePool *pool,
             void            *pool_data,
             void            *pool_local,
             void            *system_data )
{
     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
eglDestroyPool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
eglLeavePool( CoreSurfacePool *pool,
              void            *pool_data,
              void            *pool_local )
{
     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
eglTestConfig( CoreSurfacePool         *pool,
               void                    *pool_data,
               void                    *pool_local,
               CoreSurfaceBuffer       *buffer,
               const CoreSurfaceConfig *config )
{
     D_DEBUG_AT( EGL_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     return DFB_OK;
}

static DFBResult
eglAllocateBuffer( CoreSurfacePool       *pool,
                   void                  *pool_data,
                   void                  *pool_local,
                   CoreSurfaceBuffer     *buffer,
                   CoreSurfaceAllocation *allocation,
                   void                  *alloc_data )
{
     CoreSurface       *surface;
     EGLAllocationData *alloc = alloc_data;
     GLint              tex;
     GLint              fbo;

     D_DEBUG_AT( EGL_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     surface = buffer->surface;

     dfb_surface_calc_buffer_size( surface, 8, 1, &alloc->pitch, &alloc->size );

     D_DEBUG_AT( EGL_Surfaces, "  -> pitch %d\n", alloc->pitch );
     D_DEBUG_AT( EGL_Surfaces, "  -> size  %d\n", alloc->pitch );

     allocation->size   = alloc->size;
     allocation->offset = -1;

     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );
     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );

     glGenTextures( 1, &alloc->tex );

     glBindTexture( GL_TEXTURE_2D, alloc->tex );

     glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, surface->config.size.w, surface->config.size.h, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, NULL );

     glGenFramebuffers( 1, &alloc->fbo );

     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );

     glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, alloc->tex, 0 );

     glBindTexture( GL_TEXTURE_2D, tex );
     glBindFramebuffer( GL_FRAMEBUFFER, fbo );

     D_DEBUG_AT( EGL_Surfaces, "  -> tex   %u\n", alloc->tex );
     D_DEBUG_AT( EGL_Surfaces, "  -> fbo   %u\n", alloc->fbo );

     D_MAGIC_SET( alloc, EGLAllocationData );

     return DFB_OK;
}

static DFBResult
eglDeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     EGLAllocationData *alloc = alloc_data;

     D_DEBUG_AT( EGL_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );

     D_DEBUG_AT( EGL_Surfaces, "  -> pitch %d\n", alloc->pitch );
     D_DEBUG_AT( EGL_Surfaces, "  -> size  %d\n", alloc->size );
     D_DEBUG_AT( EGL_Surfaces, "  -> tex   %u\n", alloc->tex );
     D_DEBUG_AT( EGL_Surfaces, "  -> fbo   %u\n", alloc->fbo );

     glDeleteFramebuffers( 1, &alloc->fbo );
     glDeleteTextures( 1, &alloc->tex );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
eglLock( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         CoreSurfaceBufferLock *lock )
{
     EGLAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p, %p )\n", __FUNCTION__, allocation, lock->buffer );

     lock->pitch  = alloc->pitch;
     lock->offset = ~0;
     lock->addr   = NULL;
     lock->phys   = 0;

     if (lock->accessor == CSAID_GPU) {
          if (lock->access & CSAF_WRITE) {
               if (allocation->type & CSTF_LAYER)
                    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
               else
                    glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
          }
          else
               lock->handle = (void*)(long) alloc->tex;
     }

     D_DEBUG_AT( EGL_SurfLock, "  -> offset %lu, pitch %u, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
eglUnlock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     EGLAllocationData *alloc = alloc_data;

     D_UNUSED_P( alloc );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p, %p )\n", __FUNCTION__, allocation, lock->buffer );

     return DFB_OK;
}

static DFBResult
eglWrite( CoreSurfacePool       *pool,
          void                  *pool_data,
          void                  *pool_local,
          CoreSurfaceAllocation *allocation,
          void                  *alloc_data,
          const void            *source,
          int                    pitch,
          const DFBRectangle    *rect )
{
     EGLAllocationData *alloc = alloc_data;
     GLint              tex;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p )\n", __FUNCTION__, allocation->buffer );

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );

     glBindTexture( GL_TEXTURE_2D, alloc->tex );

     glTexSubImage2D( GL_TEXTURE_2D, 0, rect->x, rect->y, rect->w, rect->h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, source );

     glBindTexture( GL_TEXTURE_2D, tex );

     return DFB_OK;
}

const SurfacePoolFuncs eglSurfacePoolFuncs = {
     .AllocationDataSize = eglAllocationDataSize,
     .InitPool           = eglInitPool,
     .JoinPool           = eglJoinPool,
     .DestroyPool        = eglDestroyPool,
     .LeavePool          = eglLeavePool,
     .TestConfig         = eglTestConfig,
     .AllocateBuffer     = eglAllocateBuffer,
     .DeallocateBuffer   = eglDeallocateBuffer,
     .Lock               = eglLock,
     .Unlock             = eglUnlock,
     .Write              = eglWrite
};
