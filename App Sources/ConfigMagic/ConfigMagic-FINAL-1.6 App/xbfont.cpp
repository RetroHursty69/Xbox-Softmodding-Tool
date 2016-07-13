//-----------------------------------------------------------------------------
// File: XBFont.cpp
//
// Desc: Texture-based font class. This class reads .abc font files that are
//       generated by the FontMaker tool. These .abc files are used to create
//       a texture with all the font's glyph, and also extract information on
//       the dimensions of each glyph.
//
//       Once created, this class is used to render text in a 3D scene with the
//       following function:
//          DrawText( fScreenY, fScreenSpaceY, dwTextColor, strText,
//                    dwJustificationFlags );
//
//       For performance, you can batch mulitple DrawText calls together
//       between Begin() and End() calls, as in the following example:
//          pFont->Begin();
//          pFont->DrawText( ... );
//          pFont->DrawText( ... );
//          pFont->DrawText( ... );
//          pFont->End();
//
//       The size (extent) of the text can be computed without rendering with
//       the following function:
//          GetTextExtent( strText, pfReturnedWidth, pfReturnedHeight,
//                         bComputeExtentUsingFirstLineOnly );
//
//       Finally, the font class can create a texture to hold rendered text,
//       which is useful for static text that must be rendered for many
//       frames, or can even be used within a 3D scene. (For instance, for a
//       player's name on a jersey.) Use the following function for this:
//          CreateTexture( strText, d3dTextureFormat );
//
//       See the XDK docs for more information.
//
// Hist: 11.01.00 - New for November XDK release
//       12.15.00 - Changes for December XDK release
//       02.18.01 - Changes for March XDK release
//       04.15.01 - Using packed resources for May XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <stdio.h>
#include "XBFont.h"




// Max size for the font class' vertex buffer
const DWORD XBFONT_MAX_VERTICES = 1024 * 4;

// Helper function to init a vertex
inline XBFONTVERTEX InitFontVertex( const D3DXVECTOR4& p, D3DCOLOR color,
                                    FLOAT tu, FLOAT tv )
{
    XBFONTVERTEX v;   
    v.p     = p;   
    v.color = color;   
    v.tu    = tu;   
    v.tv    = tv;
    return v;
}




//-----------------------------------------------------------------------------
// Name: CXBFont()
// Desc: Font class constructor.
//-----------------------------------------------------------------------------
CXBFont::CXBFont()
{
    m_pd3dDevice         = NULL;
    m_pTexture           = NULL;

    for( DWORD i=0; i<NUM_FONT_BUFFERS; i++ )
        m_pVBs[i] = NULL;
    m_pVB                = NULL;
    m_dwCurrentBuffer    = 0L;
    m_bTextureFromFile   = FALSE;

    m_pVertices          = NULL;
    m_dwNumQuads         = 0L;
    m_dwNestedBeginCount = 0L;

    m_dwFontHeight       = 36;
    m_dwTexWidth         = 64;
    m_dwTexHeight        = 64;

    m_cLowChar           = 0;
    m_cHighChar          = 0;

    m_dwNumGlyphs        = 0L;
    m_Glyphs             = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CXBFont()
// Desc: Font class destructor.
//-----------------------------------------------------------------------------
CXBFont::~CXBFont()
{
    Destroy();
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Create the font's internal objects (texture and array of glyph info)
//       using the image and information from two files.
//-----------------------------------------------------------------------------
HRESULT CXBFont::Create( LPDIRECT3DDEVICE8 pd3dDevice, 
                         const CHAR* strFontResourceFileName )
{
    HRESULT hr;

    // Store the device for use in member functions
    m_pd3dDevice = pd3dDevice;

    if( FAILED( m_xprResource.Create( m_pd3dDevice, strFontResourceFileName, 
                                      2 ) ) )
        return E_FAIL;

    m_pTexture = m_xprResource.GetTexture( 0UL );

    BYTE* pData = (BYTE*)m_xprResource.GetData( XBResource_SizeOf(m_pTexture) );
    DWORD dwResourceType = ((DWORD*)pData)[0];
    DWORD dwResourceSize = ((DWORD*)pData)[1];
    pData += 2*sizeof(DWORD);

    DWORD dwVersion = ((DWORD*)pData)[0];
    m_dwFontHeight  = ((DWORD*)pData)[1];
    m_dwTexWidth    = ((DWORD*)pData)[2];
    m_dwTexHeight   = ((DWORD*)pData)[3];
    DWORD dwBPP     = ((DWORD*)pData)[4];
    pData += 5*sizeof(DWORD);

    // Check version of file (to make sure it matches up with the FontMaker tool)
    if( dwVersion != 0x00000004 )
    {
        OutputDebugStringA( "XBFont: Incorrect version number on font file!\n" );
        return E_FAIL;
    }

    // Read the low and high char
    m_cLowChar  = ((WCHAR*)pData)[0];
    m_cHighChar = ((WCHAR*)pData)[1];
    pData += 2*sizeof(WCHAR);

    // Read the glyph attributes from the file
    m_Glyphs = (GLYPH_ATTR*)(pData+4);

    // Create vertex buffer for rendering text strings
    for( DWORD i=0; i<NUM_FONT_BUFFERS; i++ )
    {
        hr = pd3dDevice->CreateVertexBuffer( XBFONT_MAX_VERTICES*sizeof(XBFONTVERTEX),
                                             D3DUSAGE_WRITEONLY, 0L,
                                             D3DPOOL_DEFAULT, &m_pVBs[i] );
        if( FAILED(hr) )
            return hr;
    }

    // Assign a current vertex buffer
    m_dwCurrentBuffer = 0L;
    m_pVB = m_pVBs[m_dwCurrentBuffer];

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Destroy()
// Desc: Destroys the font object
//-----------------------------------------------------------------------------
HRESULT CXBFont::Destroy()
{
    m_xprResource.Destroy();

    // Delete vertex buffers
    for( DWORD i=0; i<NUM_FONT_BUFFERS; i++ )
        SAFE_RELEASE( m_pVBs[i] );

    if( m_bTextureFromFile )
    {
        SAFE_RELEASE( m_pTexture );
        delete [] m_Glyphs;
    }

    m_pd3dDevice         = NULL;
    m_pTexture           = NULL;
    m_pVB                = NULL;
    m_dwCurrentBuffer    = 0L;
    m_bTextureFromFile   = FALSE;

    m_pVertices          = NULL;
    m_dwNumQuads         = 0L;
    m_dwNestedBeginCount = 0L;

    m_dwFontHeight       = 36;
    m_dwTexWidth         = 64;
    m_dwTexHeight        = 64;

    m_cLowChar           = 0;
    m_cHighChar          = 0;

    m_dwNumGlyphs        = 0L;
    m_Glyphs             = NULL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetTextExtent()
// Desc: Get the dimensions of a text string
//-----------------------------------------------------------------------------
HRESULT CXBFont::GetTextExtent( const WCHAR* strText, FLOAT* pWidth, 
                                FLOAT* pHeight, BOOL bFirstLineOnly ) const
{
    // Check parameters
    if( NULL==strText || NULL==pWidth || NULL==pHeight )
        return E_INVALIDARG;

    // Set default text extent in output parameters
    (*pWidth)   = 0.0f;
    (*pHeight)  = 0.0f;

    // Initialize counters that keep track of text extent
    FLOAT sx = 0.0f;
    FLOAT sy = (FLOAT)(m_dwFontHeight + 1);

    // Loop through each character and update text extent
    while( *strText )
    {
        WCHAR letter = *strText++;
        
        // Handle newline character
        if( letter == L'\n' )
        {
            if( bFirstLineOnly )
                return S_OK;
            sx  = 0.0f;
            sy += (FLOAT)(m_dwFontHeight + 1);
        }

        // Ignore unprintable characters
        if( letter<m_cLowChar || letter>m_cHighChar )
            continue;

        // Get text extent for this character's glyph
        GLYPH_ATTR* pGlyph = &m_Glyphs[letter - m_cLowChar];
        sx += pGlyph->wOffset;
        sx += pGlyph->wAdvance;

        // Store text extent of string in output parameters
        if( sx > (*pWidth) )   (*pWidth)  = sx;
        if( sy > (*pHeight) )  (*pHeight) = sy;
     }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Begin()
// Desc: Prepares the font vertex buffers for rendering.
//-----------------------------------------------------------------------------
HRESULT CXBFont::Begin()
{
    // Lock vertex buffer on the first call (allow nesting of begin/end calls)
    if( 0 == m_dwNestedBeginCount )
    {
        // Assign a current vertex buffer
        if( m_pVB->IsBusy() )
        {
            if( ++m_dwCurrentBuffer >= NUM_FONT_BUFFERS )
                m_dwCurrentBuffer = 0L;
            m_pVB = m_pVBs[m_dwCurrentBuffer];
        }

        // Lock the vertex buffer
        m_pVB->Lock( 0, 0, (BYTE**)&m_pVertices, 0L );
        m_dwNumQuads = 0;
    }

    // Keep track of the nested begin/end calls.
    m_dwNestedBeginCount++;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawText()
// Desc: Draws text as textured polygons
//-----------------------------------------------------------------------------
HRESULT CXBFont::DrawText( FLOAT fOriginX, FLOAT fOriginY, DWORD dwColor,
                           const WCHAR* strText, DWORD dwFlags )
{
    // Set up stuff (i.e. lock the vertex buffer) to prepare for drawing text
    Begin();

    // Set the starting screen position
    FLOAT sx = fOriginX;
    FLOAT sy = fOriginY;

    // If vertically centered, offset the starting sy value
    if( dwFlags & XBFONT_CENTER_Y )
    {
        FLOAT w, h;
        GetTextExtent( strText, &w, &h );
        sy = floorf( sy - h/2 );
    }

    // Set a flag so we can determine initial justification effects
    BOOL bStartingNewLine = TRUE;

    while( *strText )
    {
        // If starting text on a new line, determine justification effects
        if( bStartingNewLine )
        {
            if( dwFlags & (XBFONT_RIGHT|XBFONT_CENTER_X) )
            {
                // Get the extent of this line
                FLOAT w, h;
                GetTextExtent( strText, &w, &h, TRUE );

                // Offset this line's starting sx value
                if( dwFlags & XBFONT_RIGHT )
                    sx = floorf( fOriginX - w );
                if( dwFlags & XBFONT_CENTER_X )
                    sx = floorf( fOriginX - w/2 );
            }
            bStartingNewLine = FALSE;
        }

        // Get the current letter in the string
        WCHAR letter = *strText++;

        // Handle the newline character
        if( letter == L'\n' )
        {
            sx  = fOriginX;
            sy += m_dwFontHeight;
            bStartingNewLine = TRUE;
        }

        // Skip invalid characters
        if( letter<m_cLowChar || letter>m_cHighChar )
            continue;

        // Get the glyph for this character
        GLYPH_ATTR* pGlyph = &m_Glyphs[letter-m_cLowChar];

        // Setup the screen coordinates (note the 0.5f shift value which is to
        // align texel centers with pixel centers)
        sx += pGlyph->wOffset;
        FLOAT sx1 = sx - 0.5f;
        FLOAT sx2 = sx - 0.5f + ((FLOAT)pGlyph->wWidth + 1);
        FLOAT sy1 = sy - 0.5f;
        FLOAT sy2 = sy - 0.5f + ((FLOAT)m_dwFontHeight + 1);
        sx += pGlyph->wAdvance;

        // Setup the texture coordinates (note the fudge factor for converting
        // from integer texel values to floating point texture coords).
        FLOAT tx1 = ( pGlyph->left   * ( m_dwTexWidth-1) ) / m_dwTexWidth;
        FLOAT ty1 = ( pGlyph->top    * (m_dwTexHeight-1) ) / m_dwTexHeight;
        FLOAT tx2 = ( pGlyph->right  * ( m_dwTexWidth-1) ) / m_dwTexWidth;
        FLOAT ty2 = ( pGlyph->bottom * (m_dwTexHeight-1) ) / m_dwTexHeight;

        // Set up the vertices (1 quad = 2 triangles = 6 vertices)
        *m_pVertices++ = InitFontVertex( D3DXVECTOR4(sx1,sy2,0.0f,0.0f), dwColor, tx1, ty2 );
        *m_pVertices++ = InitFontVertex( D3DXVECTOR4(sx1,sy1,0.0f,0.0f), dwColor, tx1, ty1 );
        *m_pVertices++ = InitFontVertex( D3DXVECTOR4(sx2,sy1,0.0f,0.0f), dwColor, tx2, ty1 );
        *m_pVertices++ = InitFontVertex( D3DXVECTOR4(sx2,sy2,0.0f,0.0f), dwColor, tx2, ty2 );
        m_dwNumQuads++;

        // If the vertex buffer is full, render it. This will stall the GPU, so
        // we should sure that XBFONT_MAX_VERTICES is big enough.
        if( (m_dwNumQuads+1)*4 > XBFONT_MAX_VERTICES )
        {
            // Unlock, render, and relock the vertex buffer
            m_pVB->Unlock();
            Render();
            m_pVB->Lock( 0, 0, (BYTE**)&m_pVertices, 0L );
            m_dwNumQuads = 0L;
        }
    }

    // Call End() to complete the begin/end pair for drawing text
    End();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: End()
// Desc: Called after Begin(), this function triggers the rendering of the
//       vertex buffer contents filled during calls to DrawText().
//-----------------------------------------------------------------------------
HRESULT CXBFont::End()
{
    // Keep track of nested calls to begin/end.
    if( 0L == m_dwNestedBeginCount )
        return E_FAIL;
    if( --m_dwNestedBeginCount > 0 )
        return S_OK;
    
    // Unlock the vertex buffer
    m_pVB->Unlock();

    // Render the contents of the vertex buffer
    if( m_dwNumQuads > 0 )
        Render();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: The internal call to actually render the vertices.
//-----------------------------------------------------------------------------
HRESULT CXBFont::Render()
{
    // Set the necessary renderstates
    m_pd3dDevice->SetTexture( 0, m_pTexture );
    m_pd3dDevice->SetStreamSource( 0, m_pVB, sizeof(XBFONTVERTEX) );
    m_pd3dDevice->SetVertexShader( D3DFVF_XBFONTVERTEX );
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF,         0x08 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC,        D3DCMP_GREATEREQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FOGENABLE,        FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE,    FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_EDGEANTIALIAS,    FALSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

    // Render the vertex buffer
    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, m_dwNumQuads );

    // We can restore state here, if we like. Unfortunately, for generic use,
    // it's hard to guess what state the app will want everything restored to.

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderToTexture()
// Desc: Creates a texture and renders a text string into it. The texture
//       format defaults to a 32-bit linear texture
//-----------------------------------------------------------------------------
LPDIRECT3DTEXTURE8 CXBFont::CreateTexture( const WCHAR* strText, 
                                           D3DCOLOR dwBackgroundColor, 
                                           D3DCOLOR dwTextColor, 
                                           D3DFORMAT d3dFormat )
{
    // Calculate texture dimensions
    FLOAT fTexWidth;
    FLOAT fTexHeight;
    GetTextExtent( strText, &fTexWidth, &fTexHeight );
    DWORD dwWidth  = (DWORD)fTexWidth;
    DWORD dwHeight = (DWORD)fTexHeight;

    switch( d3dFormat )
    {
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
            // For swizzled textures, make sure the dimensions are a power of two
            for( DWORD wmask=1; dwWidth&(dwWidth-1); wmask = (wmask<<1)+1 )
                dwWidth  = ( dwWidth + wmask ) & ~wmask;
            for( DWORD hmask=1; dwHeight&(dwHeight-1); hmask = (hmask<<1)+1 )
                dwHeight = ( dwHeight + hmask ) & ~hmask;
            break;

        case D3DFMT_LIN_A8R8G8B8:
        case D3DFMT_LIN_X8R8G8B8:
        case D3DFMT_LIN_R5G6B5:
        case D3DFMT_LIN_X1R5G5B5:
            // For linear textures, make sure the stride is a multiple of 64 bytes
            dwWidth  = ( dwWidth + 0x1f ) & ~0x1f;
            break;

        default:
            // All other formats are unsupported as render targets
            return NULL;
    }

    // Create the texture
    LPDIRECT3DTEXTURE8 pTexture;
    if( FAILED( m_pd3dDevice->CreateTexture( dwWidth, dwHeight, 1, 0L, d3dFormat, 
                                             D3DPOOL_DEFAULT, &pTexture ) ) )
        return NULL;
    
    // Get the current backbuffer and zbuffer
    LPDIRECT3DSURFACE8 pBackBuffer, pZBuffer;
    m_pd3dDevice->GetRenderTarget( &pBackBuffer );
    m_pd3dDevice->GetDepthStencilSurface( &pZBuffer );

    // Set the new texture as the render target
    LPDIRECT3DSURFACE8 pTextureSurface;
    pTexture->GetSurfaceLevel( 0, &pTextureSurface );
    D3DVIEWPORT8 vp = { 0, 0, dwWidth, dwHeight, 0.0f, 1.0f };
    m_pd3dDevice->SetRenderTarget( pTextureSurface, NULL );
    m_pd3dDevice->SetViewport( &vp );
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, dwBackgroundColor, 1.0f, 0L );

    // Render the text
    DrawText( 0, 0, dwTextColor, strText, 0L );

    // Restore the rendertarget
    D3DVIEWPORT8 vpBackBuffer = { 0, 0, 640, 480, 0.0f, 1.0f };
    m_pd3dDevice->SetRenderTarget( pBackBuffer, pZBuffer );
    m_pd3dDevice->SetViewport( &vpBackBuffer );
    SAFE_RELEASE( pBackBuffer );
    SAFE_RELEASE( pZBuffer );
    SAFE_RELEASE( pTextureSurface );

    // Return the new texture
    return pTexture;
}




//-----------------------------------------------------------------------------
// Name: ReplaceInvalidChars()
// Desc: Replaces any character that does not have a glyph in the font with
//       the specified character value. Function is useful for replacing
//       invalid characters with the null "box" character.
//-----------------------------------------------------------------------------
VOID CXBFont::ReplaceInvalidChars( WCHAR* strUpdate, WCHAR cReplacement ) const
{
    // TCR 3-26 Unsupported Characters
    for( ; *strUpdate; ++strUpdate )
    {
        WCHAR cLetter = *strUpdate;
        if( cLetter == L'\n' )
            continue;
        if( cLetter < m_cLowChar || cLetter > m_cHighChar )
            *strUpdate = cReplacement;
    }
}



