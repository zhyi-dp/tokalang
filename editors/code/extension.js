const vscode = require('vscode');
const cp = require('child_process');
const path = require('path');
const os = require('os');
const fs = require('fs');

/**
 * @param {vscode.ExtensionContext} context
 */
function activate(context) {
    console.log('Toka extension is now active!');

    context.subscriptions.push(
        vscode.languages.registerDocumentSymbolProvider(
            { scheme: 'file', language: 'toka' },
            new TokaDocumentSymbolProvider()
        )
    );

    context.subscriptions.push(
        vscode.languages.registerDocumentFormattingEditProvider(
            { scheme: 'file', language: 'toka' },
            new TokaDocumentFormattingProvider()
        )
    );
}

class TokaDocumentSymbolProvider {
    provideDocumentSymbols(document, token) {
        return new Promise((resolve, reject) => {
            const symbols = [];

            // Regex patterns
            const fnRegex = /^\s*(?:pub\s+)?fn\s+([a-zA-Z_]\w*)/;
            const shapeRegex = /^\s*(?:pub\s+)?shape\s+([a-zA-Z_]\w*)/;
            const implRegex = /^\s*impl\s+(?:([a-zA-Z_]\w*)\s+for\s+)?([a-zA-Z_]\w*)/;
            const aliasRegex = /^\s*(?:pub\s+)?alias\s+([a-zA-Z_]\w*)/;

            for (let i = 0; i < document.lineCount; i++) {
                const line = document.lineAt(i);
                const text = line.text;

                if (text.trim() === '') continue;

                let match = fnRegex.exec(text);
                if (match) {
                    this.addSymbol(symbols, match[1], vscode.SymbolKind.Function, i, text, match[0]);
                    continue;
                }

                match = shapeRegex.exec(text);
                if (match) {
                    this.addSymbol(symbols, match[1], vscode.SymbolKind.Struct, i, text, match[0]);
                    continue;
                }

                match = implRegex.exec(text);
                if (match) {
                    const name = match[1] ? `${match[1]} for ${match[2]}` : match[2];
                    this.addSymbol(symbols, name, vscode.SymbolKind.Class, i, text, match[0]);
                    continue;
                }

                match = aliasRegex.exec(text);
                if (match) {
                    this.addSymbol(symbols, match[1], vscode.SymbolKind.Variable, i, text, match[0]);
                    continue;
                }
            }

            resolve(symbols);
        });
    }

    addSymbol(symbols, name, kind, lineIndex, lineText, matchText) {
        const range = new vscode.Range(lineIndex, 0, lineIndex, lineText.length);
        const selectionRange = new vscode.Range(lineIndex, 0, lineIndex, lineText.length);

        const symbol = new vscode.DocumentSymbol(
            name,
            matchText.trim(),
            kind,
            range,
            selectionRange
        );

        symbols.push(symbol);
    }
}

function deactivate() { }

const outputChannel = vscode.window.createOutputChannel("Toka Formatter");

class TokaDocumentFormattingProvider {
    provideDocumentFormattingEdits(document, options, token) {
        return new Promise((resolve, reject) => {
            outputChannel.appendLine(`Format requested for ${document.fileName}`);
            try {
                const text = document.getText();
                if (text.trim() === '') {
                    outputChannel.appendLine('Document is empty, ignoring.');
                    return resolve([]);
                }
                
                const tempFile = path.join(os.tmpdir(), `tokafmt_${Date.now()}.tk`);
                fs.writeFileSync(tempFile, text);
                
                // Assume the extension is running in a workspace where 'build/bin/tokafmt' exists
                // We fallback to workspace folder if available, else give explicit error
                let projectRoot = '';
                if (vscode.workspace.workspaceFolders && vscode.workspace.workspaceFolders.length > 0) {
                    projectRoot = vscode.workspace.workspaceFolders[0].uri.fsPath;
                } else {
                    outputChannel.appendLine('No workspace folders active. Assuming standalone mode might fail if tokafmt not in PATH.');
                }
                
                let tokafmtPath = 'tokafmt';
                if (projectRoot !== '') {
                    tokafmtPath = path.join(projectRoot, 'build', 'bin', 'tokafmt');
                }
                
                outputChannel.appendLine(`Using tokafmt path: ${tokafmtPath}`);
                if (projectRoot !== '' && !fs.existsSync(tokafmtPath)) {
                    vscode.window.showErrorMessage(`tokafmt not found at ${tokafmtPath}. Please run rebuild.sh in the Toka root directory.`);
                    outputChannel.appendLine('tokafmt binary missing.');
                    resolve([]);
                    return;
                }

                outputChannel.appendLine(`Running: ${tokafmtPath} ${tempFile}`);
                const result = cp.spawnSync(tokafmtPath, [tempFile], { encoding: 'utf8' });
                
                fs.unlinkSync(tempFile);

                if (result.error) {
                    throw result.error;
                }
                if (result.status !== 0) {
                    const errMsg = result.stderr || result.stdout || `Exit code ${result.status}`;
                    throw new Error(`Command failed: ${errMsg}`);
                }

                let formattedText = result.stdout;
                if (!formattedText || formattedText.trim() === '') {
                    outputChannel.appendLine('tokafmt returned empty text, no changes applied.');
                    resolve([]);
                    return;
                }
                
                outputChannel.appendLine('Successfully formatted.');
                
                const startPos = document.lineAt(0).range.start;
                const endPos = document.lineAt(document.lineCount - 1).range.end;
                const fullRange = new vscode.Range(startPos, endPos);
                
                // Verify if it actually changed to avoid unnecessary edits
                if (formattedText === text) {
                     outputChannel.appendLine('Text is identical. No edit applied.');
                     resolve([]);
                     return;
                }
                
                resolve([vscode.TextEdit.replace(fullRange, formattedText)]);
                vscode.window.showInformationMessage("Toka file formatted successfully!");
            } catch (err) {
                console.error(err);
                outputChannel.appendLine(`Error: ${err.message}`);
                vscode.window.showErrorMessage(`Toka Formatting Failed: ${err.message}`);
                resolve([]);
            }
        });
    }
}

module.exports = {
    activate,
    deactivate
};
