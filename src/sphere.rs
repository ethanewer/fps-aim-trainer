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

fn make_sphere(radius: f32, slices: u32, stacks: u32) -> (Vec<Vertex>, Vec<Normal>, Vec<u16>) {
    let mut vertices = Vec::new();
    let mut normals = Vec::new();
    let mut indices = Vec::new();
    
    let pi = std::f32::consts::PI;
    let two_pi = 2.0 * pi;
    
    for i in 0..=stacks {
        let v = i as f32 / stacks as f32;
        let phi = v * pi;
        
        for j in 0..=slices {
            let u = j as f32 / slices as f32;
            let theta = u * two_pi;
            
            let x = radius * phi.sin() * theta.cos();
            let y = radius * phi.sin() * theta.sin();
            let z = radius * phi.cos();
            
            let nx = x / radius;
            let ny = y / radius;
            let nz = z / radius;
            
            vertices.push(Vertex { position: [x, y, z] });
            normals.push(Normal { normal: [nx, ny, nz] });

            
            if i < stacks && j < slices {
                let first = (i * (slices + 1) + j) as u16;
                let second = first + slices as u16 + 1;
                
                indices.push(first);
                indices.push(second);
                indices.push(first + 1);
                
                indices.push(second);
                indices.push(second + 1);
                indices.push(first + 1);
            }
        }
    }
    
    (vertices, normals, indices)
}

pub fn get_target(display: &glium::Display) -> (glium::VertexBuffer<Vertex>, glium::VertexBuffer<Normal>, glium::IndexBuffer<u16>, glium::Program) {
    let (s_verts, s_norms, s_inds) = make_sphere(1.0, 32, 32);
    let target_positions = glium::VertexBuffer::new(display, &s_verts).unwrap();
    let target_normals = glium::VertexBuffer::new(display, &s_norms).unwrap();
    let target_indices = glium::IndexBuffer::new(display, glium::index::PrimitiveType::TrianglesList, &s_inds).unwrap();

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

    let fragment_shader = r#"
        #version 150

        in vec3 v_normal;
        out vec4 color;

        uniform vec3 u_light;

        void main() {
            float brightness = dot(normalize(v_normal), normalize(u_light));
            vec3 dark_color = vec3(0.0, 0.4, 0.8);
            vec3 regular_color = vec3(0.0, 0.5, 1.0);
            color = vec4(mix(dark_color, regular_color, brightness), 1.0);
        }
    "#;

    let target_program = glium::Program::from_source(display, vertex_shader_src, fragment_shader, None).unwrap();

    (target_positions, target_normals, target_indices, target_program)
}
