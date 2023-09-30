use glium;

#[derive(Copy, Clone)]
pub struct Vertex {
    position: [f32; 3],
}

implement_vertex!(Vertex, position);

#[derive(Copy, Clone)]
pub struct Normal {
    normal: [f32; 3],
}

implement_vertex!(Normal, normal);

pub fn get_cube(display: &glium::Display) -> (glium::VertexBuffer<Vertex>, glium::VertexBuffer<Normal>, glium::IndexBuffer<u16>, glium::Program) {
    const VERTICES: [Vertex; 24] = [
        // Front
        Vertex { position: [-1.0, -1.0, 1.0] },
        Vertex { position: [1.0, -1.0, 1.0] },
        Vertex { position: [-1.0, 1.0, 1.0] },
        Vertex { position: [1.0, 1.0, 1.0] },

        // Back
        Vertex { position: [1.0, -1.0, -1.0] },
        Vertex { position: [-1.0, -1.0, -1.0] },
        Vertex { position: [1.0, 1.0, -1.0] },
        Vertex { position: [-1.0, 1.0, -1.0] },

        // Top
        Vertex { position: [-1.0, 1.0, 1.0] },
        Vertex { position: [1.0, 1.0, 1.0] },
        Vertex { position: [-1.0, 1.0, -1.0] },
        Vertex { position: [1.0, 1.0, -1.0] },

        // Bottom
        Vertex { position: [-1.0, -1.0, -1.0] },
        Vertex { position: [1.0, -1.0, -1.0] },
        Vertex { position: [-1.0, -1.0, 1.0] },
        Vertex { position: [1.0, -1.0, 1.0] },

        // Right
        Vertex { position: [1.0, -1.0, 1.0] },
        Vertex { position: [1.0, -1.0, -1.0] },
        Vertex { position: [1.0, 1.0, 1.0] },
        Vertex { position: [1.0, 1.0, -1.0] },

        // Left
        Vertex { position: [-1.0, -1.0, -1.0] },
        Vertex { position: [-1.0, -1.0, 1.0] },
        Vertex { position: [-1.0, 1.0, -1.0] },
        Vertex { position: [-1.0, 1.0, 1.0] },
    ];

    // Define the normals for the cube
    const NORMALS: [Normal; 24] = [
        // Front
        Normal { normal: [0.0, 0.0, 1.0] },
        Normal { normal: [0.0, 0.0, 1.0] },
        Normal { normal: [0.0, 0.0, 1.0] },
        Normal { normal: [0.0, 0.0, 1.0] },

        // Back
        Normal { normal: [0.0, 0.0, -1.0] },
        Normal { normal: [0.0, 0.0, -1.0] },
        Normal { normal: [0.0, 0.0, -1.0] },
        Normal { normal: [0.0, 0.0, -1.0] },

        // Top
        Normal { normal: [0.0, 1.0, 0.0] },
        Normal { normal: [0.0, 1.0, 0.0] },
        Normal { normal: [0.0, 1.0, 0.0] },
        Normal { normal: [0.0, 1.0, 0.0] },

        // Bottom
        Normal { normal: [0.0, -1.0, 0.0] },
        Normal { normal: [0.0, -1.0, 0.0] },
        Normal { normal: [0.0, -1.0, 0.0] },
        Normal { normal: [0.0, -1.0, 0.0] },

        // Right
        Normal { normal: [1.0, 0.0, 0.0] },
        Normal { normal: [1.0, 0.0, 0.0] },
        Normal { normal: [1.0, 0.0, 0.0] },
        Normal { normal: [1.0, 0.0, 0.0] },

        // Left
        Normal { normal: [-1.0, 0.0, 0.0] },
        Normal { normal: [-1.0, 0.0, 0.0] },
        Normal { normal: [-1.0, 0.0, 0.0] },
        Normal { normal: [-1.0, 0.0, 0.0] },
    ];

    // Define the indices for the cube
    const INDICES: [u16; 36] = [
        0, 1, 2, 2, 1, 3, // front
        4, 5, 6, 6, 5, 7, // back
        8, 9, 10, 10, 9, 11, // top
        12, 13, 14, 14, 13, 15, // bottom
        16, 17, 18, 18, 17, 19, // right
        20, 21, 22, 22, 21, 23, // left
    ];

    let positions = glium::VertexBuffer::new(display, &VERTICES).unwrap();
    let normals = glium::VertexBuffer::new(display, &NORMALS).unwrap();
    let indices = glium::IndexBuffer::new(display, glium::index::PrimitiveType::TrianglesList, &INDICES).unwrap();

    let vertex_shader_src = r#"
        #version 150

        in vec3 position;
        in vec3 normal;
        out vec3 v_normal;

        uniform mat4 perspective;
        uniform mat4 view;
        uniform mat4 model;

        void main() {
            mat4 modelview = view * model;
            v_normal = transpose(inverse(mat3(modelview))) * normal;
            gl_Position = perspective * modelview * vec4(position, 1.0);
        }
    "#;

    let fragment_shader_src = r#"
        #version 150

        in vec3 v_normal;
        out vec4 color;

        uniform vec3 u_light;

        void main() {
            float brightness = dot(normalize(v_normal), normalize(u_light));
            vec3 dark_color = vec3(0.075, 0.075, 0.075);
            vec3 regular_color = vec3(0.1, 0.1, 0.1);
            color = vec4(mix(dark_color, regular_color, brightness), 1.0);
        }
    "#;

    let program = glium::Program::from_source(display, vertex_shader_src, fragment_shader_src, None).unwrap();

    (positions, normals, indices, program)
}

