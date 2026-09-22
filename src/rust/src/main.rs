//! Small runnable example for the `dao` crate - see README.md for more.
//!
//! Run with `cargo run` (with `$DAOROOT` set to an installed daoBase, or
//! from a checkout that has been `waf build`t - see build.rs).

use dao::Shm;

fn main() {
    let name = "/tmp/test_rust.im.shm";

    // Create a new 4x4 f32 SHM (or overwrite an existing one).
    let mut writer = Shm::create::<f32>(name, &[4, 4]).expect("failed to create SHM");
    println!("created {name}, shape = {:?}", writer.shape());

    writer.set_data(&[1.0f32; 16]).expect("failed to write data");
    println!("wrote a frame, counter = {}", writer.counter());

    // Attach to the same SHM from a second handle, as a separate process
    // reading it would.
    let mut reader = Shm::open(name).expect("failed to open SHM");
    let data: &[f32] = reader.get_data().expect("failed to read data");
    println!(
        "read back {} elements, sum = {}",
        data.len(),
        data.iter().sum::<f32>()
    );
}
