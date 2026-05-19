const EXAMPLES = {
    "hello": `import std/io::println

fn main() -> i32 {
    println("Welcome to the Toka Playground!")
    
    // Toka variables are declared with 'auto'
    auto name = "World"
    println("Hello, {}!", name)
    
    return 0
}`,
    "smart_ptr": `import std/io::println

shape Data(id: i32)

// Toka uses ~ for shared (reference counted) pointers
fn create_data(id: i32) -> ~Data {
    auto ~p = new Data(id = id)
    println("Created data with id: {}", p.id)
    return ~p 
}

fn consume_data(~p: Data) {
    println("Consuming data with id: {}", p.id)
}

fn main() -> i32 {
    auto ~p1 = create_data(101)
    
    // Shared pointers are reference counted automatically
    consume_data(~p1)
    
    println("After consume_data, p1 is still valid: id={}", p1.id)
    
    // p1 will be automatically dropped here!
    return 0
}`,
    "shape_match": `import std/io::println

// Tagged Union (Enum) Definition
shape MyResult(
    Ok(i32) = 0 | 
    Err(i32) = 1
)

fn process(val: i32) -> i32 {
    auto res = MyResult::Ok(val)
    
    // match expression with value-merging logic
    auto outcome = match res {
        auto MyResult::Ok(v) => {
            pass v * 2
        }
        auto MyResult::Err(e) => {
            pass 0
        }
    }
    
    return outcome
}

fn main() -> i32 {
    auto b = process(10)
    println("Result: {}", b)
    return 0
}`,
    "async_demo": `import std/task::{sleep, block_on}
import std/io::println

fn worker_a() -> async i32 {
    println(" --> [Worker A] Resumed and running background task!")
    return 0
}

fn worker_b() -> async i32 {
    println(" --> [Worker B] Resumed and running background task!")
    return 0
}

fn event_loop_sentinel() -> async i32 {
    println("--- Queue Start Discharging ---")
    sleep(100) 
    println("--- Queue Exhausted ---")
    return 0
}

pub fn main() -> async i32 {
    println("[Async Main] Spawning background Workers...")
    
    worker_a().start
    worker_b().start
    
    // Block current coroutine and drain the event loop
    block_on<i32>(event_loop_sentinel())
    
    println("[Async Main] All done!")
    return 0
}`
};

let editor;
let checkTimeout;

document.addEventListener("DOMContentLoaded", () => {
    // Initialize CodeMirror
    editor = CodeMirror.fromTextArea(document.getElementById("code-editor"), {
        mode: "toka",
        theme: "material-darker",
        lineNumbers: true,
        indentUnit: 4,
        matchBrackets: true,
        autoCloseBrackets: true,
    });

    editor.setValue(EXAMPLES["hello"]);

    // Handle Example Selection
    document.getElementById("example-select").addEventListener("change", (e) => {
        editor.setValue(EXAMPLES[e.target.value]);
        runCheck();
    });

    // Handle Check Button
    document.getElementById("check-btn").addEventListener("click", () => {
        runCheck();
    });

    // Auto-check on type (debounce)
    editor.on("change", () => {
        clearTimeout(checkTimeout);
        checkTimeout = setTimeout(runCheck, 500);
    });
});

// The Module object is populated by Emscripten
var Module = {
    onRuntimeInitialized: function() {
        document.getElementById("status-indicator").textContent = "Compiler Ready";
        document.getElementById("status-indicator").className = "status-indicator status-ok";
        runCheck();
    }
};

function runCheck() {
    if (!Module._check_toka_code) return; // Wasm not ready

    const code = editor.getValue();
    const terminal = document.getElementById("terminal");
    const status = document.getElementById("status-indicator");
    
    status.textContent = "Checking...";
    status.className = "status-indicator status-loading";
    
    // Allocate C string
    const lengthBytes = lengthBytesUTF8(code) + 1;
    const stringOnWasmHeap = _malloc(lengthBytes);
    stringToUTF8(code, stringOnWasmHeap, lengthBytes);
    
    // Call C++ function
    const resultPtr = Module._check_toka_code(stringOnWasmHeap);
    const resultJson = UTF8ToString(resultPtr);
    
    _free(stringOnWasmHeap);
    
    // Parse result
    try {
        const res = JSON.parse(resultJson);
        terminal.innerHTML = "";
        
        if (res.status === "ok") {
            status.textContent = "Syntax OK";
            status.className = "status-indicator status-ok";
            terminal.innerHTML = "<span class='diag-note'>✔ Code successfully parsed and borrow-checked!</span>";
            
            // Clear squiggles
            editor.operation(() => {
                editor.getAllMarks().forEach(mark => mark.clear());
            });
        } else {
            status.textContent = "Errors Found";
            status.className = "status-indicator status-error";
            
            // Clear old squiggles
            editor.getAllMarks().forEach(mark => mark.clear());
            
            let html = "";
            res.diagnostics.forEach(diag => {
                const isError = diag.level === 0; // Assuming 0 is error
                const className = isError ? "diag-error" : "diag-warning";
                const typeStr = isError ? "error" : "warning";
                
                html += `<div style="margin-bottom: 12px;">
                    <span class="${className}">${typeStr}[${diag.code}]</span>: ${diag.message}<br>
                    <span class="diag-file"> --> playground.tk:${diag.line}:${diag.col}</span>
                </div>`;
                
                // Add squiggly line in editor
                if (diag.line > 0) {
                    editor.markText(
                        {line: diag.line - 1, ch: diag.col > 0 ? diag.col - 1 : 0},
                        {line: diag.line - 1, ch: diag.col > 0 ? diag.col + 2 : 1},
                        {className: "cm-error"}
                    );
                }
            });
            terminal.innerHTML = html;
        }
    } catch (e) {
        status.textContent = "Compiler Error";
        status.className = "status-indicator status-error";
        terminal.textContent = "Failed to parse compiler output:\n" + resultJson;
    }
}
