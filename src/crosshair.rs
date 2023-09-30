#[derive(Copy, Clone)]
pub struct Vertex {
    position: [f32; 2],
}

implement_vertex!(Vertex, position);

const CROSSHAIR_IMAGE: &[u8] = include_bytes!("crosshair_image.png");

pub fn get_crosshair(display: &glium::Display) -> (glium::VertexBuffer<Vertex>, glium::index::NoIndices, glium::Program, [[f32; 4]; 4], glium::texture::Texture2d) {
    let image = image::load_from_memory(CROSSHAIR_IMAGE).expect("Failed to load image");
    let image_rgba = image.to_rgba8();
    let dimensions = image_rgba.dimensions();
    let raw = image_rgba.into_raw();
    let raw_image = glium::texture::RawImage2d::from_raw_rgba_reversed(&raw, dimensions);
    let texture = glium::texture::Texture2d::new(display, raw_image).unwrap();

    let vertices = vec![
        Vertex { position: [-1.0, -1.0] },
        Vertex { position: [1.0, -1.0] },
        Vertex { position: [-1.0, 1.0] },
        Vertex { position: [1.0, 1.0] },
    ];
    
    let indices = glium::index::NoIndices(glium::index::PrimitiveType::TriangleStrip);
    let positions = glium::VertexBuffer::new(display, &vertices).unwrap();

    let program = program!(display,
        140 => {
            vertex: "
                #version 140
                uniform mat4 matrix;
                in vec2 position;
                out vec2 tex_coords;
                void main() {
                    gl_Position = matrix * vec4(position, 0.0, 1.0);
                    tex_coords = position * 0.5 + 0.5;
                }
            ",
            fragment: "
                #version 140
                uniform sampler2D tex;
                in vec2 tex_coords;
                out vec4 color;
                void main() {
                    if (texture(tex, tex_coords).rgb == vec3(0.0)) {
                        discard;
                    } else {
                        color = vec4(1.0, 1.0, 1.0, 1.0);
                    }
                }
            ",
        },
    ).unwrap();

    let matrix = [
        [0.0140625, 0.0, 0.0, 0.0],
        [0.0, 0.025, 0.0, 0.0],
        [0.0, 0.0, 0.25, 0.0],
        [0.0, 0.0, 0.0, 1.0f32],
    ];

    (positions, indices, program, matrix, texture)
}