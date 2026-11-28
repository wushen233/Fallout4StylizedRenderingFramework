#ifndef FO4_PARALLAX_FACE_TRANSPORT
#define FO4_PARALLAX_FACE_TRANSPORT

// Shared RT6 keeps one t66 declaration and the existing reflected name.
namespace ExtendedMaterials
{
	Texture2D<float4> ParallaxShadowTexture : register(t66);
}

#endif
