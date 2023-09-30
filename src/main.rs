#[macro_use]
extern crate glium;
use glium::{glutin, Surface};
use rand::prelude::*;
use std::time::Instant;
use dialoguer::{theme::ColorfulTheme, Select, Input};

mod cube;
mod sphere;
mod crosshair;
mod util;
use util::*;

fn main() {
    // let mut sensitivity: f32 = 0.000122;
    let mut sensitivity: f32 = 0.00122;

    let options = [
        "Gridshot",
        "Gridshot Small",
        "Static Clicking", 
        "Static Clicking Small", 
        "Static Clicking Extra Small", 
        "Dynamic Clicking", 
        "Dynamic Clicking Small", 
        "Dynamic Clicking Extra Small", 
        "Tracking", 
        "Tracking Small",
        "Change Sensitivity",
    ];
    
    loop {
        let selection = Select::with_theme(&ColorfulTheme::default())
            .with_prompt("Select an option:")
            .items(&options[..])
            .default(0)
            .interact()
            .unwrap();

        match selection {
            0 => grid_clicking_challenge(sensitivity, 1.75, 0.0, 1.0),
            1 => grid_clicking_challenge(sensitivity, 0.875, 0.0, 0.5),
            2 => clicking(sensitivity, 0.5, 0.0),
            3 => clicking(sensitivity, 0.25, 0.0),
            4 => clicking(sensitivity, 0.125, 0.0),
            5 => clicking(sensitivity, 0.5, 0.5),
            6 => clicking(sensitivity, 0.25, 1.0),
            7 => clicking(sensitivity, 0.125, 2.0),
            8 => tracking(sensitivity, 0.5, 0.5, 2.0),
            9 => tracking(sensitivity, 0.25, 1.0, 2.0),
            10 => sensitivity = get_sensitivity(),
            _ => panic!("Invalid selection!"),
        }
    }
}

fn get_sensitivity() -> f32 {
    let sensitivity: String = Input::new()
        .with_prompt("Enter new sensitivity")
        .interact_text()
        .unwrap();

    let sensitivity: f32 = sensitivity.parse().expect("Invalid sensitivity value");

    sensitivity
}

fn clicking(mouse_sensitivity: f32, target_size: f32, target_speed: f32) {
    const NUM_TARGETS: usize = 8;

    let event_loop = glutin::event_loop::EventLoop::new();
    let wb = glutin::window::WindowBuilder::new().with_fullscreen(Some(glutin::window::Fullscreen::Borderless(None)));
    let cb = glutin::ContextBuilder::new().with_depth_buffer(8);
    let display = glium::Display::new(wb, cb, &event_loop).unwrap();
    {
        let gl_window = &display.gl_window();
        let window = gl_window.window();
        window.set_cursor_grab(glutin::window::CursorGrabMode::Confined)
                .or_else(|_e| window.set_cursor_grab(glutin::window::CursorGrabMode::Locked))
                .unwrap();
            
        window.set_cursor_visible(false);
    }

    let (map_positions, 
        map_normals, 
        map_indices, 
        map_program) = cube::get_cube(&display);

    let (s_positions, 
        s_normals, 
        s_indices, 
        s_program) = sphere::get_target(&display);

    let (crosshair_positions, 
        crosshair_indices, 
        crosshair_program, 
        crosshair_matrix, 
        crosshair_texture) = crosshair::get_crosshair(&display);

    let mut rng = rand::thread_rng();
    let mut x_pos: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];
    let mut y_pos: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];
    let mut z_pos: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];
    let mut delta_x: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];

    for i in 0..NUM_TARGETS {
        let mut good_spawn: bool = false;

        x_pos[i] = rng.gen();
        z_pos[i] = rng.gen();
        z_pos[i] *= 0.5;

        while !good_spawn {
            y_pos[i] = rng.gen();
            good_spawn = true;

            for j in 0..NUM_TARGETS {
                if i == j {
                    continue;
                }
                if (y_pos[i] - y_pos[j]).abs() < 0.075 * target_size {
                    good_spawn = false;
                }
            }
        }

        if rng.gen_bool(0.5) {
            delta_x[i] = target_speed * target_size;
        } else {
            delta_x[i] = -target_speed * target_size;
        } 
    }


    let mut last_mouse_pos = [-1.57079632679, 0.0f32];
    let mut previous_frame_time = Instant::now();

    event_loop.run(move |ev, _, control_flow| {
        let next_frame_time = std::time::Instant::now() + std::time::Duration::from_nanos(2083333);
        *control_flow = glutin::event_loop::ControlFlow::WaitUntil(next_frame_time);

        let current_time = Instant::now();
        let delta_time = current_time.duration_since(previous_frame_time).as_secs_f32();
        previous_frame_time = current_time;

        for i in 0..NUM_TARGETS {
            x_pos[i] += delta_x[i] * delta_time;


            if (x_pos[i] < -0.5 + 0.05 * target_size && delta_x[i] < 0.0) || (x_pos[i] > 1.5 - 0.05 * target_size && delta_x[i] > 0.0) {
                delta_x[i] *= -1.0;
            } 
        }

        let mut target = display.draw();
        
        let camera_position = [0.0, 0.0, 0.0f32];
        let camera_direction = compute_camera_direction(last_mouse_pos[0], last_mouse_pos[1]);
        let camera_up = compute_camera_up(&camera_direction);
        let view = view_matrix(&camera_position, &camera_direction, &camera_up);
        let perspective = {
            let (width, height) = target.get_dimensions();
            let aspect_ratio = height as f32 / width as f32;

            let fov: f32 = 0.39444 * 3.141592;
            let zfar = 1024.0;
            let znear = 0.1;

            let f = 1.0 / (fov / 2.0).tan();

            [
                [f * aspect_ratio, 0.0, 0.0, 0.0],
                [0.0, f, 0.0, 0.0],
                [0.0, 0.0,  (zfar + znear) / (zfar - znear), 1.0],
                [0.0, 0.0, -(2.0 * zfar * znear) / (zfar - znear), 0.0],
            ]
        };

        let map_uniforms = uniform! {
            model: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.75, 1.0f32],
            ],
            u_light: [1.0, 0.0, 0.0f32],
            perspective: perspective,
            view: view,
        };

        let mut target_uniforms_vec: Vec<glium::uniforms::UniformsStorage<[[f32; 4]; 4], 
            glium::uniforms::UniformsStorage<[[f32; 4]; 4], 
            glium::uniforms::UniformsStorage<[f32; 3], 
            glium::uniforms::UniformsStorage<[[f32; 4]; 4], 
            glium::uniforms::EmptyUniforms>>>>> = Vec::new();

        for i in 0..NUM_TARGETS {
            target_uniforms_vec.push(uniform! {
                model: [
                    [0.05 * target_size, 0.0, 0.0, 0.0],
                    [0.0, 0.05 * target_size, 0.0, 0.0],
                    [0.0, 0.0, 0.05 * target_size, 0.0],
                    [0.5 - x_pos[i], 0.5 - y_pos[i], 1.75 - z_pos[i], 1.0f32],
                ],
                u_light: [1.0, 0.0, 0.0f32],
                perspective: perspective,
                view: view,
            });
        }

        let crosshair_uniforms = uniform! {
            matrix: crosshair_matrix,
            tex: &crosshair_texture,
        };

        let params = glium::DrawParameters {
            depth: glium::Depth {
                test: glium::draw_parameters::DepthTest::IfLess,
                write: true,
                ..Default::default()
            },
            ..Default::default()
        };

        target.clear_color_and_depth((0.25, 0.25, 0.25, 0.25), 1.0);
        target.draw((&map_positions, &map_normals), &map_indices, &map_program, &map_uniforms, &params).unwrap();

        for i in 0..NUM_TARGETS {
            target.draw((&s_positions, &s_normals), &s_indices, &s_program, &target_uniforms_vec[i], &params).unwrap();
        }

        target.draw(&crosshair_positions, &crosshair_indices, &crosshair_program, &crosshair_uniforms, &params).unwrap();
        target.finish().unwrap();

        match ev {
            glutin::event::Event::DeviceEvent { event, .. } => match event {
                glutin::event::DeviceEvent::MouseMotion { delta } => {
                    let delta_x = delta.0 as f32 * mouse_sensitivity;
                    let delta_y = delta.1 as f32 * mouse_sensitivity;
                    last_mouse_pos[0] += delta_x;
                    last_mouse_pos[1] += delta_y;
                },
                _ => return,
            },
            glutin::event::Event::WindowEvent { event, .. } => match event {
                glutin::event::WindowEvent::KeyboardInput { input, .. } => {
                    if let Some(glutin::event::VirtualKeyCode::Escape) = input.virtual_keycode {
                        *control_flow = glutin::event_loop::ControlFlow::Exit;
                        return;
                    }
                },
                glutin::event::WindowEvent::CloseRequested => {
                    *control_flow = glutin::event_loop::ControlFlow::Exit;
                    return;
                },
                glutin::event::WindowEvent::MouseInput { button, state, .. } => {
                    if button == glutin::event::MouseButton::Left && state == glutin::event::ElementState::Pressed {
                        for i in 0..NUM_TARGETS {
                            let target_position = [0.5 - x_pos[i], 0.5 - y_pos[i], 1.75 - z_pos[i]];
                            let target_dist = dist(&target_position, &[0.0, 0.0, 0.0f32]);

                            let scaled_camera_direction = [
                                camera_direction[0] * target_dist,
                                camera_direction[1] * target_dist,
                                camera_direction[2] * target_dist,
                            ];

                            let camera_angle_dist = dist(&target_position, &scaled_camera_direction);

                            if camera_angle_dist < 0.05 * target_size {
                                let mut good_spawn: bool = false;

                                x_pos[i] = rng.gen();
                                z_pos[i] = rng.gen();
                                z_pos[i] *= 0.55;

                                while !good_spawn {
                                    y_pos[i] = rng.gen();
                                    good_spawn = true;

                                    for j in 0..NUM_TARGETS {
                                        if i == j {
                                            continue;
                                        }
                                        if (y_pos[i] - y_pos[j]).abs() < 0.075 * target_size {
                                            good_spawn = false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                _ => return,
            },
            _ => return,
        }        
    });
}

fn tracking(mouse_sensitivity: f32, target_size: f32, target_speed: f32, damage: f32) {
    const NUM_TARGETS: usize = 8;
    const TARGET_STRAFING: bool = true;
    
    let event_loop = glutin::event_loop::EventLoop::new();
    let wb = glutin::window::WindowBuilder::new().with_fullscreen(Some(glutin::window::Fullscreen::Borderless(None)));
    let cb = glutin::ContextBuilder::new().with_depth_buffer(8);
    let display = glium::Display::new(wb, cb, &event_loop).unwrap();
    {
        let gl_window = &display.gl_window();
        let window = gl_window.window();
        window.set_cursor_grab(glutin::window::CursorGrabMode::Confined)
                .or_else(|_e| window.set_cursor_grab(glutin::window::CursorGrabMode::Locked))
                .unwrap();
            
        window.set_cursor_visible(false);
    }

    let (map_positions, 
        map_normals, 
        map_indices, 
        map_program) = cube::get_cube(&display);

    let (s_positions, 
        s_normals, 
        s_indices, 
        s_program) = sphere::get_target(&display);

    let (crosshair_positions, 
        crosshair_indices, 
        crosshair_program, 
        crosshair_matrix, 
        crosshair_texture) = crosshair::get_crosshair(&display);

    let mut rng = rand::thread_rng();
    let mut hp: [f32; NUM_TARGETS] = [1.0; NUM_TARGETS];
    let mut x_pos: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];
    let mut y_pos: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];
    let mut z_pos: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];
    let mut delta_x: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];

    for i in 0..NUM_TARGETS {
        let mut good_spawn: bool = false;

        x_pos[i] = rng.gen();
        z_pos[i] = rng.gen();
        z_pos[i] *= 0.5;

        while !good_spawn {
            y_pos[i] = rng.gen();
            good_spawn = true;

            for j in 0..NUM_TARGETS {
                if i == j {
                    continue;
                }
                if (y_pos[i] - y_pos[j]).abs() < 0.075 * target_size {
                    good_spawn = false;
                }
            }
        }

        if rng.gen_bool(0.5) {
            delta_x[i] = target_speed * target_size;
        } else {
            delta_x[i] = -target_speed * target_size;
        } 
    }

    let mut last_mouse_pos = [-1.57079632679, 0.0f32];
    let mut mouse_button_pressed: bool = false;
    let mut previous_frame_time = Instant::now();

    event_loop.run(move |ev, _, control_flow| {
        let next_frame_time = std::time::Instant::now() + std::time::Duration::from_nanos(2083333);
        *control_flow = glutin::event_loop::ControlFlow::WaitUntil(next_frame_time);

        let current_time = Instant::now();
        let delta_time = current_time.duration_since(previous_frame_time).as_secs_f32();
        previous_frame_time = current_time;

        if TARGET_STRAFING {
            for i in 0..NUM_TARGETS {
                x_pos[i] += delta_x[i] * delta_time;


                if (x_pos[i] < -0.5 + 0.05 * target_size && delta_x[i] < 0.0) || (x_pos[i] > 1.5 - 0.05 * target_size && delta_x[i] > 0.0) {
                    delta_x[i] *= -1.0;
                } 
            }
        }

        let mut target = display.draw();
        
        let camera_position = [0.0, 0.0, 0.0f32];
        let camera_direction = compute_camera_direction(last_mouse_pos[0], last_mouse_pos[1]);
        let camera_up = compute_camera_up(&camera_direction);
        let view = view_matrix(&camera_position, &camera_direction, &camera_up);
        let perspective = {
            let (width, height) = target.get_dimensions();
            let aspect_ratio = height as f32 / width as f32;

            let fov: f32 = 0.39444 * 3.141592;
            let zfar = 1024.0;
            let znear = 0.1;

            let f = 1.0 / (fov / 2.0).tan();

            [
                [f * aspect_ratio, 0.0, 0.0, 0.0],
                [0.0, f, 0.0, 0.0],
                [0.0, 0.0,  (zfar + znear) / (zfar - znear), 1.0],
                [0.0, 0.0, -(2.0 * zfar * znear) / (zfar - znear), 0.0],
            ]
        };

        let map_uniforms = uniform! {
            model: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.75, 1.0f32],
            ],
            u_light: [1.0, 0.0, 0.0f32],
            perspective: perspective,
            view: view,
        };

        let mut target_uniforms_vec: Vec<glium::uniforms::UniformsStorage<[[f32; 4]; 4], 
            glium::uniforms::UniformsStorage<[[f32; 4]; 4], 
            glium::uniforms::UniformsStorage<[f32; 3], 
            glium::uniforms::UniformsStorage<[[f32; 4]; 4], 
            glium::uniforms::EmptyUniforms>>>>> = Vec::new();

        for i in 0..NUM_TARGETS {
            target_uniforms_vec.push(uniform! {
                model: [
                    [0.05 * target_size, 0.0, 0.0, 0.0],
                    [0.0, 0.05 * target_size, 0.0, 0.0],
                    [0.0, 0.0, 0.05 * target_size, 0.0],
                    [0.5 - x_pos[i], 0.5 - y_pos[i], 1.75 - z_pos[i], 1.0f32],
                ],
                u_light: [1.0, 0.0, 0.0f32],
                perspective: perspective,
                view: view,
            });
        }

        let crosshair_uniforms = uniform! {
            matrix: crosshair_matrix,
            tex: &crosshair_texture,
        };

        let params = glium::DrawParameters {
            depth: glium::Depth {
                test: glium::draw_parameters::DepthTest::IfLess,
                write: true,
                ..Default::default()
            },
            ..Default::default()
        };

        target.clear_color_and_depth((0.25, 0.25, 0.25, 0.25), 1.0);
        target.draw((&map_positions, &map_normals), &map_indices, &map_program, &map_uniforms, &params).unwrap();

        for i in 0..NUM_TARGETS {
            target.draw((&s_positions, &s_normals), &s_indices, &s_program, &target_uniforms_vec[i], &params).unwrap();
        }

        target.draw(&crosshair_positions, &crosshair_indices, &crosshair_program, &crosshair_uniforms, &params).unwrap();
        target.finish().unwrap();

        if mouse_button_pressed {
            for i in 0..NUM_TARGETS {
                let target_position = [0.5 - x_pos[i], 0.5 - y_pos[i], 1.75 - z_pos[i]];
                let target_dist = dist(&target_position, &[0.0, 0.0, 0.0f32]);

                let scaled_camera_direction = [
                    camera_direction[0] * target_dist,
                    camera_direction[1] * target_dist,
                    camera_direction[2] * target_dist,
                ];

                let aim_dist = dist(&target_position, &scaled_camera_direction);

                if aim_dist < 0.05 * target_size {
                    hp[i] -= damage * delta_time;

                    if hp[i] <= 0.0 {
                        let mut good_spawn: bool = false;

                        x_pos[i] = rng.gen();
                        z_pos[i] = rng.gen();
                        z_pos[i] *= 0.55;

                        while !good_spawn {
                            y_pos[i] = rng.gen();
                            good_spawn = true;

                            for j in 0..NUM_TARGETS {
                                if i == j {
                                    continue;
                                }
                                if (y_pos[i] - y_pos[j]).abs() < 0.075 * target_size {
                                    good_spawn = false;
                                }
                            }
                        }

                        hp[i] = 1.0;
                    }
                }
            }
        }

        match ev {
            glutin::event::Event::DeviceEvent { event, .. } => match event {
                glutin::event::DeviceEvent::MouseMotion { delta } => {
                    let delta_x = delta.0 as f32 * mouse_sensitivity;
                    let delta_y = delta.1 as f32 * mouse_sensitivity;
                    last_mouse_pos[0] += delta_x;
                    last_mouse_pos[1] += delta_y;
                },
                _ => return,
            },
            glutin::event::Event::WindowEvent { event, .. } => match event {
                glutin::event::WindowEvent::KeyboardInput { input, .. } => {
                    if let Some(glutin::event::VirtualKeyCode::Escape) = input.virtual_keycode {
                        *control_flow = glutin::event_loop::ControlFlow::Exit;
                        return;
                    }
                },
                glutin::event::WindowEvent::CloseRequested => {
                    *control_flow = glutin::event_loop::ControlFlow::Exit;
                    return;
                },
                glutin::event::WindowEvent::MouseInput { button, state, .. } => {
                    if button == glutin::event::MouseButton::Left && state == glutin::event::ElementState::Pressed {
                        mouse_button_pressed = true;
                    }

                    if button == glutin::event::MouseButton::Left && state == glutin::event::ElementState::Released {
                        mouse_button_pressed = false;
                    }
                }
                _ => return,
            },
            _ => return,
        }        
    });
}

fn grid_clicking_challenge(mouse_sensitivity: f32, target_size: f32, target_speed: f32, grid_scale: f32) {
    const NUM_TARGETS: usize = 3;

    let event_loop = glutin::event_loop::EventLoop::new();
    let wb = glutin::window::WindowBuilder::new().with_fullscreen(Some(glutin::window::Fullscreen::Borderless(None)));
    let cb = glutin::ContextBuilder::new().with_depth_buffer(8);
    let display = glium::Display::new(wb, cb, &event_loop).unwrap();
    {
        let gl_window = &display.gl_window();
        let window = gl_window.window();
        window.set_cursor_grab(glutin::window::CursorGrabMode::Confined)
                .or_else(|_e| window.set_cursor_grab(glutin::window::CursorGrabMode::Locked))
                .unwrap();
            
        window.set_cursor_visible(false);
    }

    let (map_positions, 
        map_normals, 
        map_indices, 
        map_program) = cube::get_cube(&display);

    let (s_positions, 
        s_normals, 
        s_indices, 
        s_program) = sphere::get_target(&display);

    let (crosshair_positions, 
        crosshair_indices, 
        crosshair_program, 
        crosshair_matrix, 
        crosshair_texture) = crosshair::get_crosshair(&display);

    let mut rng = rand::thread_rng();
    let mut x_pos: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];
    let mut y_pos: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];
    let mut delta_x: [f32; NUM_TARGETS] = [0.0; NUM_TARGETS];

    for i in 0..NUM_TARGETS {
        let mut good_spawn: bool = false;

        while !good_spawn {
            x_pos[i] = (rng.gen::<f32>() * 5.0).floor() / 5.0 + 0.1;
            y_pos[i] = (rng.gen::<f32>() * 5.0).floor() / 5.0 + 0.1;
            good_spawn = true;

            for j in 0..NUM_TARGETS {
                if i == j {
                    continue;
                }
                if x_pos[i] == x_pos[j] && y_pos[i] == y_pos[j] {
                    good_spawn = false;
                }
            }
        }
        
        if rng.gen_bool(0.5) {
            delta_x[i] = target_speed * target_size;
        } else {
            delta_x[i] = -target_speed * target_size;
        } 
    }

    let start_time = Instant::now();
    let mut previous_frame_time = Instant::now();
    let mut last_mouse_pos = [-1.57079632679, 0.0f32];
    let mut hits: usize = 0;
    let mut shots: usize = 0;
    let mut printed_score: bool = false;

    event_loop.run(move |ev, _, control_flow| {
        let next_frame_time = std::time::Instant::now() + std::time::Duration::from_nanos(2083333);
        *control_flow = glutin::event_loop::ControlFlow::WaitUntil(next_frame_time);

        let current_time = Instant::now();
        let total_time = current_time.duration_since(start_time).as_secs_f32();

        if total_time > 60.0 {
            *control_flow = glutin::event_loop::ControlFlow::Exit;
            if !printed_score {
                println!("{} targets hit in 60 seconds\n{}% accuracy", hits, 100.0 * (hits as f32 / shots as f32));
                printed_score = true;
            }
            return;
        }

        let delta_time = current_time.duration_since(previous_frame_time).as_secs_f32();
        previous_frame_time = current_time;

        for i in 0..NUM_TARGETS {
            x_pos[i] += delta_x[i] * delta_time;


            if (x_pos[i] < -0.5 + 0.05 * target_size && delta_x[i] < 0.0) || (x_pos[i] > 1.5 - 0.05 * target_size && delta_x[i] > 0.0) {
                delta_x[i] *= -1.0;
            } 
        }

        let mut target = display.draw();
        
        let camera_position = [0.0, 0.0, 0.0f32];
        let camera_direction = compute_camera_direction(last_mouse_pos[0], last_mouse_pos[1]);
        let camera_up = compute_camera_up(&camera_direction);
        let view = view_matrix(&camera_position, &camera_direction, &camera_up);
        let perspective = {
            let (width, height) = target.get_dimensions();
            let aspect_ratio = height as f32 / width as f32;

            let fov: f32 = 0.39444 * 3.141592;
            let zfar = 1024.0;
            let znear = 0.1;

            let f = 1.0 / (fov / 2.0).tan();

            [
                [f * aspect_ratio, 0.0, 0.0, 0.0],
                [0.0, f, 0.0, 0.0],
                [0.0, 0.0,  (zfar + znear) / (zfar - znear), 1.0],
                [0.0, 0.0, -(2.0 * zfar * znear) / (zfar - znear), 0.0],
            ]
        };

        let map_uniforms = uniform! {
            model: [
                [1.0, 0.0, 0.0, 0.0],
                [0.0, 1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0, 0.0],
                [0.0, 0.0, 0.75, 1.0f32],
            ],
            u_light: [1.0, 0.0, 0.0f32],
            perspective: perspective,
            view: view,
        };

        let mut target_uniforms_vec: Vec<glium::uniforms::UniformsStorage<[[f32; 4]; 4], 
            glium::uniforms::UniformsStorage<[[f32; 4]; 4], 
            glium::uniforms::UniformsStorage<[f32; 3], 
            glium::uniforms::UniformsStorage<[[f32; 4]; 4], 
            glium::uniforms::EmptyUniforms>>>>> = Vec::new();

        for i in 0..NUM_TARGETS {
            target_uniforms_vec.push(uniform! {
                model: [
                    [0.05 * target_size, 0.0, 0.0, 0.0],
                    [0.0, 0.05 * target_size, 0.0, 0.0],
                    [0.0, 0.0, 0.05 * target_size, 0.0],
                    [(0.5 - x_pos[i]) * grid_scale, (0.5 - y_pos[i]) * grid_scale, 1.75, 1.0f32],
                ],
                u_light: [1.0, 0.0, 0.0f32],
                perspective: perspective,
                view: view,
            });
        }

        let crosshair_uniforms = uniform! {
            matrix: crosshair_matrix,
            tex: &crosshair_texture,
        };

        let params = glium::DrawParameters {
            depth: glium::Depth {
                test: glium::draw_parameters::DepthTest::IfLess,
                write: true,
                ..Default::default()
            },
            ..Default::default()
        };

        target.clear_color_and_depth((0.25, 0.25, 0.25, 0.25), 1.0);
        target.draw((&map_positions, &map_normals), &map_indices, &map_program, &map_uniforms, &params).unwrap();

        for i in 0..NUM_TARGETS {
            target.draw((&s_positions, &s_normals), &s_indices, &s_program, &target_uniforms_vec[i], &params).unwrap();
        }

        target.draw(&crosshair_positions, &crosshair_indices, &crosshair_program, &crosshair_uniforms, &params).unwrap();
        target.finish().unwrap();

        match ev {
            glutin::event::Event::DeviceEvent { event, .. } => match event {
                glutin::event::DeviceEvent::MouseMotion { delta } => {
                    let delta_x = delta.0 as f32 * mouse_sensitivity;
                    let delta_y = delta.1 as f32 * mouse_sensitivity;
                    last_mouse_pos[0] += delta_x;
                    last_mouse_pos[1] += delta_y;
                },
                _ => return,
            },
            glutin::event::Event::WindowEvent { event, .. } => match event {
                glutin::event::WindowEvent::KeyboardInput { input, .. } => {
                    if let Some(glutin::event::VirtualKeyCode::Escape) = input.virtual_keycode {
                        *control_flow = glutin::event_loop::ControlFlow::Exit;
                        if !printed_score {
                            println!("{} targets hit in 60 seconds\n{}% accuracy", hits, 100.0 * (hits as f32 / shots as f32));
                            printed_score = true;
                        }
                        return;
                    }
                },
                glutin::event::WindowEvent::CloseRequested => {
                    *control_flow = glutin::event_loop::ControlFlow::Exit;
                    if !printed_score {
                        println!("{} targets hit in 60 seconds\n{}% accuracy", hits, 100.0 * (hits as f32 / shots as f32));
                        printed_score = true;
                    }
                    return;
                },
                glutin::event::WindowEvent::MouseInput { button, state, .. } => {
                    if button == glutin::event::MouseButton::Left && state == glutin::event::ElementState::Pressed {
                        shots += 1;

                        for i in 0..NUM_TARGETS {
                            let target_position = [(0.5 - x_pos[i]) * grid_scale, (0.5 - y_pos[i]) * grid_scale, 1.75];
                            let target_dist = dist(&target_position, &[0.0, 0.0, 0.0f32]);

                            let scaled_camera_direction = [
                                camera_direction[0] * target_dist,
                                camera_direction[1] * target_dist,
                                camera_direction[2] * target_dist,
                            ];

                            let camera_angle_dist = dist(&target_position, &scaled_camera_direction);

                            if camera_angle_dist < 0.05 * target_size {
                                hits += 1;

                                let mut good_spawn: bool = false;
                                let prev_x = x_pos[i];
                                let prev_y = y_pos[i];

                                while !good_spawn {
                                    x_pos[i] = (rng.gen::<f32>() * 5.0).floor() / 5.0 + 0.1;
                                    y_pos[i] = (rng.gen::<f32>() * 5.0).floor() / 5.0 + 0.1;
                                    good_spawn = true;

                                    if x_pos[i] == prev_x && y_pos[i] == prev_y {
                                        good_spawn = false;
                                    }
                        
                                    for j in 0..NUM_TARGETS {
                                        if i == j {
                                            continue;
                                        }
                                        if x_pos[i] == x_pos[j] && y_pos[i] == y_pos[j] {
                                            good_spawn = false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                _ => return,
            },
            _ => return,
        } 
    });
}