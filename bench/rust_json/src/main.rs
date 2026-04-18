use std::fs;
use std::time::Instant;

fn main() {
    println!("Reading Benchmark Payload (12MB) into Memory...");
    let payload = fs::read_to_string("/tmp/large.json").expect("Failed to read generic JSON payload!");
    let length = payload.len();

    println!("Engaging Native Dynamic JSON Parser...");
    let start = Instant::now();
    // Use dynamic generic Value to mimic Toka's generic JsonNode implementation
    let _dyn_root: serde_json::Value = serde_json::from_str(&payload).expect("JSON Parse Failed");
    let diff = start.elapsed().as_millis();

    println!("--- Rust JSON Engine Benchmark ---");
    println!("Bytes Processed: {}", length);
    println!("Dynamic Document Parse Time: {} ms", diff);
}
