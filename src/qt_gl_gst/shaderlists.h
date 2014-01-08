#ifndef SHADERLISTS_H
#define SHADERLISTS_H

#include "glwidget.h"

#define NUM_SHADERS_BRICKGLES       2
extern GLShaderModule BrickGLESShaderList[NUM_SHADERS_BRICKGLES];

#ifdef VIDI420_SHADERS_NEEDED
/* I420 */
#define NUM_SHADERS_VIDI420_NOEFFECT_NORMALISED       3
extern GLShaderModule VidI420NoEffectNormalisedShaderList[NUM_SHADERS_VIDI420_NOEFFECT_NORMALISED];

#define NUM_SHADERS_VIDI420_LIT_NORMALISED       3
extern GLShaderModule VidI420LitNormalisedShaderList[NUM_SHADERS_VIDI420_LIT_NORMALISED];

#define NUM_SHADERS_VIDI420_NOEFFECT       3
extern GLShaderModule VidI420NoEffectShaderList[NUM_SHADERS_VIDI420_NOEFFECT];

#define NUM_SHADERS_VIDI420_COLOURHILIGHT       3
extern GLShaderModule VidI420ColourHilightShaderList[NUM_SHADERS_VIDI420_COLOURHILIGHT];

#define NUM_SHADERS_VIDI420_COLOURHILIGHTSWAP       3
extern GLShaderModule VidI420ColourHilightSwapShaderList[NUM_SHADERS_VIDI420_COLOURHILIGHTSWAP];

#define NUM_SHADERS_VIDI420_ALPHAMASK       3
extern GLShaderModule VidI420AlphaMaskShaderList[NUM_SHADERS_VIDI420_ALPHAMASK];
#endif

#ifdef VIDUYVY_SHADERS_NEEDED
/* UYVY */
#define NUM_SHADERS_VIDUYVY_NOEFFECT_NORMALISED       3
extern GLShaderModule VidUYVYNoEffectNormalisedShaderList[NUM_SHADERS_VIDUYVY_NOEFFECT_NORMALISED];

#define NUM_SHADERS_VIDUYVY_LIT_NORMALISED       3
extern GLShaderModule VidUYVYLitNormalisedShaderList[NUM_SHADERS_VIDUYVY_LIT_NORMALISED];

#define NUM_SHADERS_VIDUYVY_NOEFFECT       3
extern GLShaderModule VidUYVYNoEffectShaderList[NUM_SHADERS_VIDUYVY_NOEFFECT];

#define NUM_SHADERS_VIDUYVY_COLOURHILIGHT       3
extern GLShaderModule VidUYVYColourHilightShaderList[NUM_SHADERS_VIDUYVY_COLOURHILIGHT];

#define NUM_SHADERS_VIDUYVY_COLOURHILIGHTSWAP       3
extern GLShaderModule VidUYVYColourHilightSwapShaderList[NUM_SHADERS_VIDUYVY_COLOURHILIGHTSWAP];

#define NUM_SHADERS_VIDUYVY_ALPHAMASK       3
extern GLShaderModule VidUYVYAlphaMaskShaderList[NUM_SHADERS_VIDUYVY_ALPHAMASK];
#endif


#endif // SHADERLISTS_H
