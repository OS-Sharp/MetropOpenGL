#version 450 core
out vec4 FragColor;
in vec3 color;
in vec2 texCoord;
in vec3 normal;

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;

uniform int Frame;

vec3 LinearToInverseGamma(vec3 rgb, float gamma);
vec3 ACESFilm(vec3 x);

void main() {

    vec4 color = texture(tex0, texCoord);
    //color += texture(tex1, texCoord);

    float c = color.a / 1.0f;

    vec3 gamma = ACESFilm(color.rgb);
    gamma = LinearToInverseGamma(gamma.rgb, 2.4);

    //FragColor = vec4(currentColor.rgb,1);
    FragColor = vec4(color.rgb,1);
}


vec3 LinearToInverseGamma(vec3 rgb, float gamma)
{
    //rgb = clamp(rgb, 0.0, 1.0);
    return mix(pow(rgb, vec3(1.0 / gamma)) * 1.055 - 0.055, rgb * 12.92, vec3(lessThan(rgb, 0.0031308.xxx)));
}
 
// ACES tone mapping curve fit to go from HDR to LDR
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}