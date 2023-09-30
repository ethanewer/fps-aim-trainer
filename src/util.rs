pub fn view_matrix(position: &[f32; 3], direction: &[f32; 3], up: &[f32; 3]) -> [[f32; 4]; 4] {
    let f = {
        let f = direction;
        let len = f[0] * f[0] + f[1] * f[1] + f[2] * f[2];
        let len = len.sqrt();
        [f[0] / len, f[1] / len, f[2] / len]
    };

    let s = [up[1] * f[2] - up[2] * f[1],
             up[2] * f[0] - up[0] * f[2],
             up[0] * f[1] - up[1] * f[0]];

    let s_norm = {
        let len = s[0] * s[0] + s[1] * s[1] + s[2] * s[2];
        let len = len.sqrt();
        [s[0] / len, s[1] / len, s[2] / len]
    };

    let u = [f[1] * s_norm[2] - f[2] * s_norm[1],
             f[2] * s_norm[0] - f[0] * s_norm[2],
             f[0] * s_norm[1] - f[1] * s_norm[0]];

    let p = [-position[0] * s_norm[0] - position[1] * s_norm[1] - position[2] * s_norm[2],
             -position[0] * u[0] - position[1] * u[1] - position[2] * u[2],
             -position[0] * f[0] - position[1] * f[1] - position[2] * f[2]];

    [
        [s_norm[0], u[0], f[0], 0.0],
        [s_norm[1], u[1], f[1], 0.0],
        [s_norm[2], u[2], f[2], 0.0],
        [p[0], p[1], p[2], 1.0],
    ]
}

pub fn compute_camera_direction(delta_x: f32, delta_y: f32) -> [f32; 3] {
    let yaw = -delta_x;
    let pitch = -delta_y;

    let mut new_dir = [yaw.cos() * pitch.cos(), pitch.sin(), yaw.sin() * pitch.cos()];
    normalize(&mut new_dir);

    new_dir
}

pub fn compute_camera_up(direction: &[f32; 3]) -> [f32; 3] {
    let mut right = cross(direction, &[0.0, 1.0, 0.0]);
    normalize(&mut right);

    let mut new_up = cross(&right, direction);
    normalize(&mut new_up);

    new_up
}

pub fn cross(a: &[f32; 3], b: &[f32; 3]) -> [f32; 3] {
    let mut out = [0.0; 3];
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
    out
}

pub fn normalize(vec: &mut [f32; 3]) {
    let r: f32 = (vec[0].powf(2.0) + vec[1].powf(2.0) + vec[2].powf(2.0)).powf(0.5);
    vec[0] /= r;
    vec[1] /= r;
    vec[2] /= r;
}

pub fn dist(a: &[f32; 3], b: &[f32; 3]) -> f32 {
    ((a[0] - b[0]).powf(2.0) + (a[1] - b[1]).powf(2.0) + (a[2] - b[2]).powf(2.0)).powf(0.5)
}