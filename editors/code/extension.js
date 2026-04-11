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

class TokaDocumentFormattingProvider {
    provideDocumentFormattingEdits(document, options, token) {
        return new Promise((resolve, reject) => {
            try {
                const text = document.getText();
                const tempFile = path.join(os.tmpdir(), `tokafmt_${Date.now()}.tk`);
                fs.writeFileSync(tempFile, text);
                
                // Assume the extension is running in a workspace where 'build/bin/tokafmt' exists
                const projectRoot = vscode.workspace.workspaceFolders ? vscode.workspace.workspaceFolders[0].uri.fsPath : '';
                const tokafmtPath = path.join(projectRoot, 'build', 'bin', 'tokafmt');
                
                if (!fs.existsSync(tokafmtPath)) {
                    vscode.window.showErrorMessage('tokafmt not found! Please run rebuild.sh in the Toka root directory.');
                    resolve([]);
                    return;
                }

                cp.execSync(`"${tokafmtPath}" "${tempFile}"`);
                
                const formattedText = fs.readFileSync(tempFile, 'utf8');
                fs.unlinkSync(tempFile);
                
                const fullRange = new vscode.Range(
                    document.positionAt(0),
                    document.positionAt(text.length)
                );
                
                resolve([vscode.TextEdit.replace(fullRange, formattedText)]);
            } catch (err) {
                console.error(err);
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
