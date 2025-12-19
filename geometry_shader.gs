#version 330 core
layout (points) in;//输入点
layout (triangle_strip, max_vertices = 5) out;//输出至多5个三角形

//在几何着色器中声明相同的接口块（使用一个不同的接口名）
in VS_OUT {
    vec3 color;
} gs_in[];

out vec3 fColor;

void build_house(vec4 position)
{    
    //每次我们调用EmitVertex时，gl_Position中的向量会被添加到图元中来。
    //当EndPrimitive被调用时，所有发射出的(Emitted)顶点都会合成为指定的输出渲染图元。
    //在一个或多个EmitVertex调用之后重复调用EndPrimitive能够生成多个图元。
    fColor = gs_in[0].color; // gs_in[0] 因为只有一个输入顶点
    gl_Position = position + vec4(-0.2, -0.2, 0.0, 0.0); // 1:左下  
    EmitVertex();   
    gl_Position = position + vec4( 0.2, -0.2, 0.0, 0.0); // 2:右下
    EmitVertex();
    gl_Position = position + vec4(-0.2,  0.2, 0.0, 0.0); // 3:左上
    EmitVertex();
    gl_Position = position + vec4( 0.2,  0.2, 0.0, 0.0); // 4:右上
    EmitVertex();
    gl_Position = position + vec4( 0.0,  0.4, 0.0, 0.0); // 5:顶部
    fColor = vec3(1.0, 1.0, 1.0);
    EmitVertex();
    EndPrimitive();
}

void main() {    
    build_house(gl_in[0].gl_Position);
}