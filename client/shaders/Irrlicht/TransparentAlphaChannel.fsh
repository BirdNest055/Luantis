#version 100

precision mediump float;

/* Uniforms */

uniform float uAlphaRef;
uniform int uTextureUsage0;
uniform sampler2D uTextureUnit0;
uniform int uFogEnable;
uniform int uFogType;
uniform vec4 uFogColor;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uFogDensity;

/* Varyings */

varying vec2 vTextureCoord0;
varying vec4 vVertexColor;
varying float vFogCoord;

float computeFog()
{
        const float LOG2 = 1.442695;
        float FogFactor = 0.0;

        if (uFogType == 0) // Exp
        {
                FogFactor = exp2(-uFogDensity * vFogCoord * LOG2);
        }
        else if (uFogType == 1) // Linear
        {
                float Scale = 1.0 / (uFogEnd - uFogStart);
                FogFactor = (uFogEnd - vFogCoord) * Scale;
        }
        else if (uFogType == 2) // Exp2
        {
                FogFactor = exp2(-uFogDensity * uFogDensity * vFogCoord * vFogCoord * LOG2);
        }

        FogFactor = clamp(FogFactor, 0.0, 1.0);

        return FogFactor;
}

void main()
{
        vec4 Color = vVertexColor;

        if (bool(uTextureUsage0))
        {
                Color *= texture2D(uTextureUnit0, vTextureCoord0);

                // NOTE: uAlphaRef currently controls a hard alpha-test threshold (discard if below).
                // Ideally it should control the sharpness of the alpha cutoff (e.g., smoothstep
                // transition width) rather than a binary discard. To implement: replace the
                // hard discard with smoothstep(uAlphaRef - sharpness, uAlphaRef + sharpness, Color.a)
                // where sharpness is a new uniform (default ~0.01 for near-binary behavior).
                // The current binary discard works for most cases but causes aliased edges on
                // partially-transparent geometry.
                if (Color.a < uAlphaRef)
                        discard;
        }

        if (bool(uFogEnable))
        {
                float FogFactor = computeFog();
                vec4 FogColor = uFogColor;
                // Note: Robust against a bogus FogColor.a or FogFactor
                Color.rgb = mix(FogColor.rgb, Color.rgb, FogFactor);
        }

        gl_FragColor = Color;
}
