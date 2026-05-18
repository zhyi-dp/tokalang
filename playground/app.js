const EXAMPLES = {
    "hello": `import std/io::println

fn main() -> i32 {
    println("Hello, Toka Playground!")
    return 0
}`,
    "websocket": `import std/io::println
import std/net/http
import std/net/ws

fn handle_connection(req: http::Request, ws^: ws::WebSocket) {
    println("Client connected!")
    while true {
        let msg = await ws.recv()
        if msg == none { break }
        
        // Echo back
        await ws.send(msg)
    }
}

fn main() -> i32 {
    println("Starting WebSocket server on port 8080...")
    // In Playground this is syntax-checked only, won't actually bind port
    return 0
}`,
    "async_http": `import std/io::println
import std/net/http

async fn handler(req: http::Request) -> http::Response {
    return http::Response(
        status=200,
        body="<h1>Hello from Toka Async!</h1>"
    )
}

fn main() -> i32 {
    let server^ = new http::Server(port=3000)
    server.router.get("/", handler)
    
    // server.listen() blocks until stopped
    return 0
}`,
    "linked_list": `import std/io::println

shape Node {
    val: i32
    next: ^Node?
}

fn main() -> i32 {
    auto head^ = new Node(val=10, next=null)
    auto second^ = new Node(val=20, next=null)
    
    // Move 'second' into 'head.next'
    head.next = second
    
    // Ownership transferred! second is no longer valid here
    // let x = second.val // This would be a compiler error!
    
    println("Success!")
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
