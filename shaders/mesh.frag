#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"


layout (location = 0) in vec3 in_normal;
layout (location = 1) in vec3 in_color;
layout (location = 2) in vec2 in_UV;

layout (location = 0) out vec4 outFragColor;

void main()
{
    float light_value = max(dot(in_normal, scene_data.sunlight_direction.xyz), 0.1f);

    vec3 color = in_color * texture(color_texture, in_UV).xyz;
    vec3 ambient = color * scene_data.ambient_color.xyz;

    outFragColor = vec4(color * light_value * scene_data.sunlight_color.w + ambient, 1.0f);
}